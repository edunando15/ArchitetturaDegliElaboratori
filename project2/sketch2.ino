/*
Il programma funziona anche in caso di numerosi segnali inviati: non stampa nulla, poiche' si verificano degli errori;
aspetta quindi un po' di tempo e torna di nuovo operativo. Infatti, cliccando ripetutamente (e velocemente) sul pulsante
"send" del ricevitore IR, si possono mandare numerosi segnali, i quali pero' possono generare degli errori, dai quali e'
bene che il programma sia in grado di "difendersi". Infatti vengono interpretati come segnali sovrapposti e
quindi sbagliati.
*/



#define nb 4 //numero di bytes trasmessi

const uint16_t inL = 18000; // 9000*2
const uint16_t inH = 9000; // 4500*2
const uint16_t tempoL = 1052; //526*2
const uint16_t tempoH = 3375; //1687.5*2

const uint8_t pin = 2;
volatile uint8_t bits[nb];
volatile uint8_t stato1 = 0;
volatile uint16_t m0 = 0;
volatile uint8_t cont = 0;
volatile uint8_t flag = 0;
volatile uint8_t ovfCont = 0; //contatore overflow TIMER1



void setup() {
  DDRD &= ~_BV(pin);
  PORTD |= (_BV(pin));

  TCCR1A = 0; //Timer Counter Control Register 1A

  //le seguenti 3 istruzioni permettono di settare il prescaler ad 8.
  TCCR1B &= ~_BV(CS12);
  TCCR1B |= _BV(CS11);
  TCCR1B &= ~_BV(CS10);

  EICRA |= _BV(ISC00); //External interrupt control register A
  EICRA &= ~_BV(ISC01); //questo comando e il precedente fanno attivare l'interrupt quando cambia il segnale

  EIMSK |= _BV(INT0); //attiva l'interrupt 0

  TIMSK1 |= _BV(TOIE1); //attiva l'interrupt di overflow di TIMER1
  
  Serial.begin(9600); 
  /*si utilizza la trasmissione seriale per poter visualizzare i risultati.
  E' possibile riattivare le linee commentate in basso che mostravano eventuali messaggi di errore.
  */
}



void loop() {
 if(cont == 32 && stato1 != 5) {
   Serial.print("address: ");
   Serial.println(bits[0]);
   Serial.print("¬ address: ");
   Serial.println(bits[1]);
   Serial.print("command: ");
   Serial.println(bits[2]);
   Serial.print("¬ command: ");
   Serial.println(bits[3]);
   Serial.println();
   azzera();
 }
}



void azzera() {
  for(int i=0; i<nb; i++) bits[i] = 0;
  cont = 0;
  stato1 = 0;
  EIMSK |= _BV(INT0); //abilito l'interrupt 0
}



ISR(INT0_vect) { //interrupt grazie al quale vengono letti i valori
  switch(stato1) {
    case 0: TCNT1 = 0; stato1 = 1; break; //comincia la trasmissione, il segnale e' low

    case 1: //il segnale e' high
      if(TCNT1 >= (inL - 200) && TCNT1 <= (inL + 200)) { //controlla il tempo in cui e' stato low
        stato1 = 2;
        TCNT1 = 0;
      }else {
        stato1 = 5;
      }
      break;

    case 2:
      if(TCNT1 >= (inH - 200) && TCNT1 <= (inH + 200)) { //controlla il tempo in cui e' stato high
        stato1 = 3;
        TCNT1 = 0;
      }else {
        stato1 =  5;
      }
      break;
    
    case 3: waitingForLow(); break;

    case 4: waitingForHigh(); break;

    case 5: EIMSK &= ~_BV(INT0); break; //stacco l'interrupt, e' uno stato di errore
  }
}
/*
Il metodo deve fare la lettura vera e propria e stabilire se il bit inviato vale 0 o 1 tramite i
microsecondi che sono passati.
*/
void waitingForHigh() { //stato1 = 4, il segnale ricevuto e' basso
  m0 = TCNT1;
  TCNT1 = 0;
  if( (m0 >= (tempoL - 12)) && (m0 <= (tempoL + 12)) ) flag = 0; //in questi due if controlla il tempo in cui e' stato high
  else {
    if( (m0 >= (tempoH - 20)) && (m0 <= (tempoH + 20)) ) flag = 0x80;
    else {
      stato1 = 5;
      return;
    }
  }
  shifter();
}



void waitingForLow() {//stato1 = 3, il segnale ricevuto e' alto
  m0 = TCNT1;
  TCNT1 = 0; //il Timer Counter Register comincia a contare da qui.
  if( (m0 >= (tempoL - 12)) && (m0 <= (tempoL + 12)) ) stato1 = 4; //controlla il tempo in cui e' stato low
  else {
    stato1 = 5;
  }
  if(cont == 32) { //significa che e' finita la trasmissione
    stato1 = 0;
    if(bits[0] != (uint8_t)~bits[1] || bits[2] != (uint8_t)~bits[3]) { //controllo che i bit siano complementati
      stato1 = 5;
    }
    return;
  }
}



void shifter() {
  bits[cont/8] >>= 1;
  bits[cont/8] |= flag;
  cont++;
  stato1 = 3;
}



ISR(TIMER1_OVF_vect) {
  if(stato1 == 5) { //stato di errore
    cont = 0;
    if(ovfCont++ == 5) { //entra piu' volte nell'interrupt, va in overflow ogni 32ms circa
      azzera();
      ovfCont = 0;
    }
    return;
  }
  if(stato1 != 0) {
    stato1 = 5;
    ovfCont = 0;
  }
}
