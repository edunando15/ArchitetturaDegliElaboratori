/*
La trasmissione si svolge nella seguente maniera:
<magic-code>  è il primo byte ricevuto, e funge da “password”: se non è corretto, la ricezione si interrompe. Ovviamente, la “password” va stabilita a priori.
<numero-bytes> è il secondo byte ricevuto. I primi due bit di questo byte indicano quanti saranno i byte-codice successivi (codificando 00 = 1; 01 = 2; 10 = 3; 11 = 4);
i restanti 6 sono parte di codice. Il numero massimo di bytes-codice che è possibile inviare è 4, escluso il magic-code.
Se si vuole trasmettere un numero inferiore a 1 o superiore a 4 di byte-codice, la ricezione si interrompe.
<codice> … <codice> è la serie di bytes che rappresentano il codice (da un minimo di 1 ad un massimo di 4). 
<checksum> è il byte finale, somma dei precedenti modulo 256.
In totale ci sono, quindi, da un minimo di 4 ad un massimo di 7 bytes.
*/

/*
Il codice funziona, e imposta automaticamente i valori da dare in output in maniera che siano
coerenti e che non diano problemi. E' tuttavia possibile modificare i seguenti parametri:
-il numero di bit mandati in output cambiando il valore della variabile globale numBit (riga 40) e commentando la riga 67;
-il valore del checksum, commentando la riga 68, e andando a inserire il checksum dove si vuole nell'array1, in base a numBit
(o anche no, ma in tal caso ci sara' quasi sicuramente un errore);
Il programma potrebbe non stampare i risultati attesi se vengono modificati i parametri sopra elencati
in maniera non coerente al protocollo stabilito. 
*/

//variabili di output

#define lowHighL 0xE4
#define lowHighH 0xFB //0xFBE4 in decimale corrisponde a -1052

#define high1L 0xE0
#define high1H  0xF2 //0xF2E0 in decimale corrisponde a -3360

#define startLowL 0xC0
#define startLowH 0xE0 //0xE0C0 in decimale corrisponde a -8000

#define startHighL 0x60
#define startHighH 0xF0 //0xF060 in decimale corrisponde a -4000

const uint8_t outPin = 12; //pin di output
volatile uint8_t array1 [7];
volatile uint8_t stato1 = 0;
volatile uint8_t outCont = 0;
volatile uint8_t numBit = 0; //numero di bit che vengono trasmessi

//variabili di input

#define password 0x80 //magic-code
#define nb 7 //numero di bytes trasmessi, al massimo 7

const uint16_t inL = 250; // 4000/16
const uint16_t inH = 125; // 2000/16
const uint16_t tempoL = 33; //526/16
const uint16_t tempoH = 105; //1680/16
const uint8_t inPin = 2; //pin di input

volatile uint8_t bits[nb];
volatile uint8_t stato2 = 0;
volatile uint8_t m0 = 0;
volatile uint8_t cont = 0;
volatile uint8_t flag = 0;
volatile uint8_t contaBytes = 0;


void setup() {
  Serial.begin(9600);
  //I valori da trasmettere possono essere cambiati. Se non vengono rispettate le regole di trasmissione, non ci sara' alcun risultato.
  array1[0] = 0x80;
  array1[1] = 0xFF; //questo byte deve contenere nelle prime due cifre piu' significative il numero di byte 
  array1[2] = 0x5C;
  array1[3] = 0xFC;
  array1[4] = 0x00;
  array1[5] = 0x00;
  array1[6] = 0xF5;
  numBit = ((array1[1] >> 6) + 4)*8;
  array1[numBit/8 - 1] = checksum(array1);

  DDRB |= _BV(outPin-8); //configura outPin come output
  PORTB |= _BV (outPin-8); //imposta outPin ad high

  DDRD &= ~_BV(inPin); //configura inPin come input
  PORTD |= _BV(inPin); //in particolare, input pullup

  TCNT1 = 0; //Timer Counter 1
  TCCR1A = 0; //Timer Counter Control Register 1A

  //le seguenti 3 istruzioni impostano il prescaler di Timer Counter Control Register 0B a 256
  TCCR0B &= ~_BV(CS00);
  TCCR0B &= ~_BV(CS01);
  TCCR0B |= _BV(CS02);

  //le seguenti 3 istruzioni impostano il prescaler di Timer Counter Control Register 1B a 8
  TCCR1B &= ~_BV(CS10);
  TCCR1B |= _BV(CS11); 
  TCCR1B &= ~_BV(CS12);

  EICRA |= _BV(ISC00); //External Interrupt Control Register A
  EICRA &= ~_BV(ISC01); //questo comando e il precedente fanno attivare l'interrupt quando cambia il segnale 
  EIMSK |= _BV(INT0); //attiva l'interrupt 0
  TIMSK1 |= _BV(TOIE1); //Timer Counter 1 Interrupt Mask Register, attiva l'interrupt che si avvia quando TCNT1 va in overflow

  TCNT1H = startLowH; //Timer Counter 1 High, permette di accedere alla "parte alta" di TCNT1, che in realta' e' diviso in 2 registri da 8 bit
  TCNT1L = startLowL; //Timer Counter 1 Low, permette di accere alla "parte bassa" di TCNT1

  PORTB &= ~_BV(outPin-8); //imposta outPin a low
}



void loop() {
  switch(stato1) {
    case 1:
      PORTB |= _BV(outPin-8); break;
    case 2:
      PORTB &= ~_BV(outPin-8); break;
  }

  if(cont == (contaBytes*8)) { //a ricezione finita
    for(uint8_t i = 0; i < contaBytes; i++) Serial.println(bits[i], HEX);
    cont = 0;
  }
}


//routine che si avvia quando Timer 1 va in overflow
ISR (TIMER1_OVF_vect) {
  if (outCont == numBit) {
    if (stato1 == 1) {      
      TCNT1H += lowHighH;
      TCNT1L += lowHighL;
      stato1 = 2;
    }
    else {
      stato1 = 1;
      TIMSK1 &= ~_BV(TOIE1);
    }
    return;
  }

  switch(stato1) {
    case 0:
      stato1 = 1;
      TCNT1 += 0;
      TCNT1H += startHighH;
      TCNT1L += startHighL;
      break;

    case 1:
      TCNT1H += lowHighH;
      TCNT1L += lowHighL;
      stato1 = 2;
      break;

    case 2:
      if (array1[outCont/8] & (1 << (outCont%8))) {
        TCNT1H += high1H; 
        TCNT1L += high1L;
      }
      else {
        TCNT1H += lowHighH;
        TCNT1L += lowHighL;
      }
      outCont++;
      stato1 = 1;
      break;
  }
}



//PARTE DI INPUT

void azzera() {
  for(int i = 0; i < nb; i++) bits[i] = 0;
  cont = 0;
  stato2 = 0;
  EIMSK |= _BV(INT0); //abilito l'interrupt
}


//routine che si avvia quando cambia il segnale al pin 2
ISR(INT0_vect) {
  switch(stato2) {
    case 0: TCNT0 = 0; stato2 = 1; break;

    case 1: //nel caso 1 e 2 controllo che i tempi pre-trasmissione siano corretti
      if(TCNT0 >= (inL - 1) && TCNT0 <= (inL +1)) {
        stato2 = 2;
        TCNT0 = 0;
      }else stato2 = 5;
      break;

    case 2: 
      if(TCNT0 >= (inH - 1) && TCNT0 <= (inH + 1)) {
        stato2 = 3;
        TCNT0 = 0;
      }else stato2 = 5;
      break;

    case 3: waitingForLow(); break;

    case 4: waitingForHigh(); break;
    
    case 5: EIMSK &= ~_BV(INT0); break; //stacco l'interrupt, e' uno stato di errore
  }
}



void waitingForHigh() { //mi trovo con un segnale basso, stato2 = 4
  m0 = TCNT0;
  TCNT0 = 0;
  if( (m0 >= (tempoL - 3)) && (m0 <= (tempoL + 3)) ) flag = 0;
  else {
    if( (m0 >= (tempoH - 3)) && (m0 <= (tempoH + 3)) ) flag = 0x80;
    else {
      stato2 = 5;
      return;
    }
  }
  shifter();
}



void waitingForLow() {//stato2 = 3, il segnale che si riceve e' alto.
  m0 = TCNT0;
  TCNT0 = 0; //il timer counter register comincia a contare da qui.
  if( (m0 >= (tempoL - 3)) && (m0 <= (tempoL + 3)) ) stato2 = 4;
  else stato2 = 5;
}



void shifter() {
  bits[cont/8] >>= 1;
  bits[cont/8] |= flag;

  checker();
  cont++;

  if(cont == (8*contaBytes)) {
    if(checksum(bits) != bits[contaBytes - 1]) {
      stato2 = 5;
      cont = 0;
    }else stato2 = 0;
    return;
  }

  stato2 = 3;
}



void checker() { //effettua vari controlli, tra cui se il primo byte corrisponde al magic code
  switch(cont) {
    case 7:
      if(bits[0] != password) {
        stato2 = 5;
        EIMSK &= ~_BV(INT0); // stacco l'interrupt
      }
      break;
    
    case 15:
      contaBytes = (bits[1]>>6) + 4; //faccio lo shift a dx del numero di 6 posizioni, e ne salvo il valore
      break;
    
    default: break;
  }
}



uint8_t checksum(uint8_t a[]) {
  uint8_t checkSum = 0;
  for(uint8_t i = 0; i < numBit/8 - 1; i++) checkSum += a[i];
  return checkSum;
}