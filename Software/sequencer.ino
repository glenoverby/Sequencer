/*
 *      Sequencer and Pulsed Relay Driver
 *  
 *  Copyright 2006, 2019, 2022-2023 Glen Overby, All Rights Reserved
 *
 *  This program may be freely used and distributed on a non-commercial
 *  basis.
 *
 * Version 2.0: Arduino version
 *
 * This program is designed to drive an output sequence using four output
 * ports on an Atmel(R) Tiny AVR microprocessor.  The typical use for this
 * is to sequence amateur radio transverters and amplifiers or preamplifiers.
 *
 * The port delays (and on time for the pulsed ports) is selected in 1ms
 * intervals.
 *
 * This program relies on the Timer One library from 
 *  https://github.com/PaulStoffregen/TimerOne.git
 *
 *      Memory          Output  Function
 *      Location
 * Mode
 *      This is a bit mask of:
 *
 *      Postive         0x000   Port is set to '0' when PTT is on
 *      Negative        0x001   Port is set to '1' when PTT is on
 *      Pulse On        0x002   Port is pulsed when PTT turns on
 *      Pulse Off       0x004   Port is pulsed when PTT turns off
 *
 *      Note that the combination of Pulse and Postive/Negative is permitted.
 *
 * Delays are in milliseconds (1 khz)
 *
 * The configuration I use is: 
 *                      ------- delay ---------
 *      Output  type    on      off     pulse
 *      0       POS     0       75      0       VN2222 to Ground
 *      1       POS     75      0       0       VN2222 to Ground
 *      2       P-ON-N  1       0       255     IRF9Z14 to 12V
 *      3       P-OFF-N 0       1       255     IRF9Z14 to 12V
 *
 *
 *      avrdude -p t45 -c bsd -E noreset,novcc -U
 *      eeprom:w:00,75,00,00,03,01,00,255,05,00,01,255,00,00,75,00:m
 *             | M  N  F  P| M  N  F  P | M  N  F  P | M  N  F  P |
 *             | 1111111111| 2222222222 | 3333333333 | 4444444444 |
 *
 * Tiny45
 *      Ports:
 *              PORT B 0        output 2 (2nd from top: VN2222)
 *              PORT B 1        output 3 (3rd from top: IRF9Z14)
 *              PORT B 2        output 4 (4th from top: IRF9Z14)
 *              PORT B 3        PTT
 *              PORT B 4        output 1 (top of board: VN2222)
 *
 *      The outputs that connect to a VN2222 are negative (0 = on)
 *      The outputs that connect to a IRF9Z14 are positive (1 = on)
 *
 * Arduino Mega328p
 *  CPU Arduino Function
 *  PD0 0       RX
 *  PD1 1       TX
 *  PD2 2       D2        1 - Low
 *  PD3 3       D3        2 - Low
 *  PD4 4       D4        3 - Low
 *  PD5 5       D5        4 - Low
 *  PD6 6       D6        5 - Low
 *  PD7 7       D7        6 - Low
 *  PB0 8       D8        1 - Pulse Low On
 *  PB1 9       D9        2 - Pulse Low On
 *  PB2 10      D10       1 - Pulse Low Off
 *  PB3 11      MOSI      2 - Pulse Low Off
 *  PB4 12      MISO      PTT-L Input
 *  PB5 13      SCK & LED
 *  PB6         Xtal 1
 *  PB7         Xtal 2
 *  PC0 14      A0        1 - High
 *  PC1 15      A1        2 - High
 *  PC2 16      A2        3 - High
 *  PC3 17      A3        4 - High
 *  PC4 18      A4        5 - High
 *  PC5 19      A5        6 - High
 *
 * AdaFruit ItsyBitsy Mega32u4
 *  ports A2 - A5 don't seem to function as outputs.
 */
#include <TimerOne.h>

#define DEBUG
#define PTT_PIN 7    /* PTT input */

unsigned char ptt_pin = 0;  /* PTT pin state */
unsigned char target = 0; /* target state: true is on, false is off */

volatile unsigned char  events;   /* Events set by timer interrupt */
#define  E_TIMER 0x01


void clock_tick(void)
{
  events |= E_TIMER;
}

/*
 * Per-Channel data:
 *  mode: POS, NEG, PULSE-ON, PULSE-OFF
 *  on-delay
 *  off-delay
 *  on-time for pulse ports
 */
struct portinfo {
  unsigned char mode;
  unsigned char ondelay;  /* delay from PTT on to Port on */
  unsigned char offdelay; /* delay from PTT off to Port off */
  unsigned char pulse;  /* Pulse on time */
  unsigned char pin;  /* which Arduino pin */
  unsigned char state;  /* pin's current state */
};

/* Portinfo mode values
  These are a bitmask, such that PORT_NEG can be ORed with either of the PULSE
  options.
 */
#define PORT_POS        0x000   /* Port is "On" when it is a 0 */
#define PORT_NEG        0x001   /* Port is "On" when it it a 1 */
#define PORT_PULSE_ON   0x002
#define PORT_PULSE_OFF  0x004
#define PORT_PULSE_MASK (PORT_PULSE_ON|PORT_PULSE_OFF)

#define PORT_H  PORT_NEG        /* Port is "On" when it is a 1 */
#define PORT_L  PORT_POS        /* Port is "On" when it is a 0 */

/* Portinfo State
   The portinfo state holds a value saying if the pin is on or off, not the
   actual value on the pin (0 or 1).
 */
#define PIN_ON    1
#define PIN_OFF   0

#define PTT_ON    0x010
#define PTT_OFF   0x020
#define PORT_TIMER  0x040
#define PORT_ABORT  0x080 /* PTT changed partway through sequence */

struct portinfo portinfo[] = {
#if 0
  // ATMega328 - General Purpose
  //{ PORT_PULSE_ON | PORT_NEG, 1, 1, 250, LED_BUILTIN, 0 },
  //{ PORT_PULSE_OFF | PORT_NEG, 1, 1, 1, LED_BUILTIN, 0 },
  { PORT_NEG, 0, 0, 0, LED_BUILTIN, 0 }, /* LED mirrors PTT status */
  { PORT_POS,  0, 50, 0, 2, 0 },
  { PORT_POS, 10, 40, 0, 3, 0 },
  { PORT_POS, 20, 30, 0, 4, 0 },
  { PORT_POS, 30, 20, 0, 5, 0 },
  { PORT_POS, 40, 10, 0, 6, 0 },
  { PORT_POS, 50,  0, 0, 7, 0 },
  { PORT_PULSE_ON | PORT_L,  0,  0, 250, 8, 0 },
  { PORT_PULSE_ON | PORT_POS, 10, 10, 250, 9, 0 },
  { PORT_PULSE_OFF | PORT_L,  0,  0, 250, 10, 0 },
  { PORT_PULSE_OFF | PORT_POS, 10, 10, 250, 11, 0 },
  { PORT_NEG,  0,  0, 0, 14, 0 }, // A0
  { PORT_NEG, 10, 10, 0, 15, 0 }, // A1
  { PORT_NEG, 20, 20, 0, 16, 0 }, // A2
  { PORT_NEG, 30, 30, 0, 17, 0 }, // A3
  { PORT_NEG, 40, 40, 0, 18, 0 }, // A4
  { PORT_NEG, 50, 50, 0, 19, 0 }, // A5
  #define UART Serial
#endif
#if 1
  // ATMega328 - Sequencer 4+2 board
  //{ PORT_PULSE_ON | PORT_NEG, 1, 1, 250, LED_BUILTIN, 0 },
  //{ PORT_PULSE_OFF | PORT_NEG, 1, 1, 1, LED_BUILTIN, 0 },
  { PORT_NEG, 0, 0, 0, LED_BUILTIN, 0 }, /* LED mirrors PTT status */
  { PORT_H,    0, 40, 0, 8, 0 },
  { PORT_H,   10, 30, 0, 9, 0 },
  { PORT_H,   20, 20, 0, 10, 0 },
  { PORT_H,   30, 10, 0, 12, 0 },

  // Pulsed relay driver - P channel Mosfet
  { PORT_PULSE_ON  | PORT_H,  40,  0, 250, 2, 0 },
  { PORT_PULSE_OFF | PORT_H,  0,  50, 250, 3, 0 },
  // T/R Relay driver
  //{ PORT_H,  0,  50, 0, 2, 0 },
  //{ PORT_H,  10, 50, 0, 3, 0 },
  // RX/TX Power
  //{ PORT_H,  0,  50, 0, 2, 0 },
  //{ PORT_L,  10, 50, 0, 3, 0 },
  #define UART Serial
#endif
#if 0
  // Adafruit ItsyBitsy - 
  { PORT_NEG,  0,  0, 0, LED_BUILTIN, 0 }, /* LED mirrors PTT status */
  { PORT_NEG,  0, 50, 0, 2, 0 },
  { PORT_NEG, 10, 40, 0, 3, 0 },
  { PORT_NEG, 20, 30, 0, 4, 0 },
  { PORT_NEG, 30, 20, 0, 5, 0 },
  { PORT_NEG, 40, 10, 0, 6, 0 },
  { PORT_NEG, 50,  0, 0, 7, 0 },
  { PORT_PULSE_ON | PORT_POS,  0,  0, 250, 8, 0 },
  { PORT_PULSE_ON | PORT_POS, 10, 10, 250, 9, 0 },
  { PORT_PULSE_OFF | PORT_POS,  0,  0, 250, 10, 0 },
  { PORT_PULSE_OFF | PORT_POS, 10, 10, 250, 11, 0 },
  { PORT_NEG,  0, 50, 0, 14, 0 }, // A0
  { PORT_NEG, 10, 40, 0, 15, 0 }, // A1
  { PORT_NEG, 20, 30, 0, 16, 0 }, // A2
  { PORT_NEG, 30, 20, 0, 17, 0 },
  { PORT_NEG, 40, 10, 0, 18, 0 },
  { PORT_NEG, 50,  0, 0, 19, 0 },
  #define UART SerialUSB
#endif
#if 0
  // Adafruit ItsyBitsy - 6m driver amp
  { PORT_NEG,  0,  0, 0, LED_BUILTIN, 0 }, /* LED mirrors PTT status */
  { PORT_NEG,  0, 40, 0, 2, 0 },    // P2 - T/R Relay
  { PORT_NEG, 20, 20, 0, 3, 0 },    // P3 - Driver Amp VCC
  { PORT_NEG, 40,  0, 0, 4, 0 },    // P4 - PTT Out (to main amp)
  { PORT_POS,  0,  0, 0, 5, 0 },    // P5 - mirror PTT
  { PORT_NEG,  0,  0, 0, 7, 0 },    // P7 - mirror PTT
  #define UART SerialUSB
#endif
#if 0
  // Arduino Clone Mega32u4 - Breadboard
  { PORT_NEG, 00, 00, 0, LED_BUILTIN, 0 }, /* LED mirrors PTT status */
  { PORT_H, 00, 40, 0, 2, 0 },    // P2
  { PORT_L, 20, 20, 0, 3, 0 },    // P3
  { PORT_PULSE_ON | PORT_H,  40, 10, 250, 4, 0 }, // P4
  { PORT_PULSE_OFF | PORT_H,  0,  0, 250, 5, 0 }, // P5
  #define UART SerialUSB
#endif
#if 0
  // SparkFun PowerDriver Shield (DEV-10618)
  { PORT_NEG,  0,  0, 0, LED_BUILTIN, 0 }, /* LED mirrors PTT status */
  { PORT_NEG,  0, 50, 0, 3, 0 },
  { PORT_NEG, 10, 40, 0, 5, 0 },
  { PORT_NEG, 20, 30, 0, 6, 0 },
  { PORT_NEG, 30, 20, 0, 9, 0 },
  { PORT_NEG, 40, 10, 0, 10, 0 },
  { PORT_NEG, 50,  0, 0, 11, 0 },
  #define UART Serial
#endif
};
unsigned char timers[24];


/*
 * Change port state: turn on or off.  (Version 3)
 * 
 * POS          set port bit to 0 to turn on
 * 
 * NEG          set port bit to 1 to turn on
 * 
 * PTT          is 0 when 'activated' (PTT-L)
 * 
 * PIN          the current state of the output pin
 * 
 * Timer        is True when call came from the timer
 * 
 * ToPin        is what to set the pin to
 * 
 * Timer        is 1 if the timer should be restarted
 * 
 * 
 * MODE         PTT     Timer   PIN     ToPin   StTimer
 * ------------------------------------------------------------
 * POS          0                       0       0
 * POS          1                       1       0
 * 
 * P-ON POS     0               1       0       1
 * P-ON POS     0               0       1       0
 * P-ON POS     1                       1       0
 * 
 * P-OFF POS    0                       1       0
 * P-OFF POS    1               1       0       1
 * P-OFF POS    1               0       1       0
 * 
 * NEG          0                       1       0
 * NEG          1                       0       0
 * 
 * P-ON NEG     0               0       1       1       
 * P-ON NEG     0               1       0       0
 * P-ON NEG     1                       0       0
 * 
 * P-OFF NEG    0                       0       0
 * P-OFF NEG    1               1       0       0
 * P-OFF NEG    1               0       1       1
 * 
 * 
 * Make this logic table POS/NEG neutral:  (To is the state to set the pin to)
 * 
 * POS          on                      on      no      To=PTT
 * POS          off                     off     no      To=PTT
 * 
 * P-ON         on              off     on      yes     To=PTT&!PIN
 * P-ON         on              on      off     no
 * P-ON         off             any     off     no
 * 
 * P-OFF        on              any     off     no
 * P-OFF        off             off     on      yes     To=!PTT&!PIN
 * P-OFF        off             on      off     no
 * 
 * To convert the on/off logc to a 1 or 0 on the port, respecting the POS/NEG
 * pin state:
 * 
 * PIN  =               ToPIN   mode&PORT_NEG
 * ----------------------------------------
 * 0    on (T)          0       POS
 * 1    on (T)          1       NEG             To && !pos
 * 1    off (F)         0       POS
 * 0    off (F)         1       NEG             !To && pos
 */
char
turn(unsigned char port, char what)
{
  unsigned char
    mode,   /* portinfo mode */
    state,  /* current pin state True = On */
    ptt,    /* PTT state.  True = On */
    to,     /* Set port this state True = On */
    r,      /* state to return, True if a pulsed port is turned on */
    pos;    /* true if port is on when it is a 1 */

  to = r = 0;

  mode = portinfo[port].mode;
  pos = (mode&(PORT_NEG)) == PORT_POS; /* true if port is Positive */
  state = portinfo[port].state;
  ptt = what; /* True when 'on' */

#ifdef DEBUG
  UART.print("Turn ");
  UART.print(port);
  UART.print(" ");
  UART.print(ptt);
  UART.print(" ");
  UART.print(pos);
  UART.print(" ");
  UART.print(state);
  UART.print(" - ");
#endif

  switch(mode&(~PORT_NEG)) {
  case PORT_PULSE_ON:    
    to = r = ptt && !state;  // if state is on, turn pulsed port off
    state = to;
    break;
  case PORT_PULSE_OFF:
    to = r = !ptt && !state; // if state is on, turn pulsed port off
    state = to;
    break;
  default:
    to = state = ptt;
  }

#ifdef DEBUG
  UART.print(state);
  UART.print(" ");
  UART.print(to);
  UART.print(" ");
  UART.println(r);
#endif
  
  if ( (to && !pos) || (!to && pos) ) {
#ifdef DEBUG
    UART.print("H: ");
    UART.print(port);
    UART.print(" ");
    UART.println(portinfo[port].pin);
#endif
    digitalWrite(portinfo[port].pin, HIGH); // on
  } else {
#ifdef DEBUG
    UART.print("L: ");
    UART.print(port);
    UART.print(" ");
    UART.print(portinfo[port].pin);
    UART.println();
#endif
    digitalWrite(portinfo[port].pin, LOW);  // off
  }
  portinfo[port].state = state;
  return r;
}

char
check_ptt()
{
  int ptt;
  ptt = digitalRead(PTT_PIN);
  if (ptt != ptt_pin) {     /* If changed, debounce */
    delay(25);
    if(digitalRead(PTT_PIN) == ptt) {
      ptt_pin = ptt;
#ifdef DEBUG
      UART.print("PTT ");
      UART.println(ptt);
#endif
      return(1);    /* PTT has changed */
    }
  }
  return(0);
}

/*
 * start_sequence is run when the PTT input state changes.
 * It changes the port state and, if needed, initializes the timers
 */
void
start_sequence(unsigned char ptt)
{
  /*
   * PTT is True when on
   */
  //unsigned char ptt;    /* State of the PTT pin */
  //unsigned char pcbits;
  int d;                /* on/off delay */
  int p;                /* port counter */
  char    *t;           /* Timer */
  struct portinfo *info;

  target = ptt; /* set target state for turn() and timers */

  for (p = 0, info = portinfo, t = timers;
    p < sizeof(portinfo)/sizeof(struct portinfo);
    p++, info++, t++) {

    d = (ptt) ? info->ondelay : info->offdelay;
    if (*t || !d) {
      if(turn(p, target)) {
        /* turn() returns non-zero if the port is a pulsed port that is on */
        *t = info->pulse;
      }
    } else {
#ifdef DEBUG
      UART.print("Delay ");
      UART.print(p);
      UART.print(" "); 
      UART.println(d);
#endif
      *t = d;
    }
  }
}

/* Process timers */
int
run_timers()
{
  struct portinfo *info;
  int p;
  char *t;
  int n = 0;
  
  if (events) {
    events = 0;
    for (p = 0, info = portinfo, t = timers;
      p < sizeof(portinfo)/sizeof(struct portinfo);
      p++, info++, t++) {

      if (*t == 0) {
        continue;
      }
      n++;
      if (--(*t) == 0) { /* Timer expired */
#ifdef TIMERDEBUG
        UART.print("Timer "),
        UART.print(p);
        UART.println(" Expired ");
#endif
        if(turn(p, target)) {
          /* Returns true if the port is a pulsed port that is on */
          *t = info->pulse;
        }
      }
    }
  } else {
#ifdef TIMERDEBUG
    Serial.println("No Timer Events");
#endif
  }
  return n;
}

/* Syntax:
  pinMode(pin#, INPUT);
  pinMode(pin#, INPUT_PULLUP);
  pinMode(pin#, OUTPUT);
  digitalWrite(pin#, state);
  pinMode(LED_BUILTIN, OUTPUT);
 */

void setup() {
  struct portinfo *pi;
  int n;

  UART.begin(9600);
  UART.println("Start");

  // Initialize all timers to zero
  for (n=0; n < sizeof(timers); n++) {
    timers[n]=0;
  }
  
  pinMode(PTT_PIN, INPUT_PULLUP);
  ptt_pin = digitalRead(PTT_PIN); /* Initialize PTT input */
  
  Timer1.initialize(1000);
  Timer1.attachInterrupt(clock_tick);

  UART.println("port configuration");

  /* Initialize ports based off of the portinfo configuration */
  for(pi=portinfo, n=0; 
    n < sizeof(portinfo)/sizeof(struct portinfo);
    pi++, n++) {
    pi->state = PIN_OFF;
    UART.print(n);
    UART.print(" ");
    UART.print(pi->mode);
    UART.print(" ");
    UART.print(pi->ondelay);
    UART.print(" ");
    UART.print(pi->offdelay);
    UART.print(" ");
    UART.print(pi->pulse);
    UART.print(" ");
    UART.print(pi->pin);
    UART.print(" ");
    UART.print(pi->state);
    UART.println("");
    pinMode(pi->pin, OUTPUT);
    if (pi->mode & PORT_NEG) {
      digitalWrite(pi->pin, LOW);
    } else {
      digitalWrite(pi->pin, HIGH);
    }
  }
}

void print_port_info()
{
  struct portinfo *pi;
  int n;
  /* Initialize ports based off of the portinfo configuration */
  for(pi=portinfo, n=0; 
    n < sizeof(portinfo)/sizeof(struct portinfo);
    pi++, n++) {
    UART.print(n);
    UART.print(" ");
    UART.print(pi->mode);
    UART.print(" ");
    UART.print(pi->ondelay);
    UART.print(" ");
    UART.print(pi->offdelay);
    UART.print(" ");
    UART.print(pi->pulse);
    UART.print(" ");
    UART.print(pi->pin);
    UART.print(" ");
    UART.print(pi->state);
    UART.println("");
  }
}

void loop() {
  // digitalRead(pin#);
  if (events) {
    run_timers();
    events = 0;
  }

  if (check_ptt()) {
    start_sequence(ptt_pin == 0);
  }

#if 1
  // send data only when you receive data:
  if (UART.available() > 0) {
    // read the incoming byte:
    int incomingByte;
    incomingByte = UART.read();

    // say what you got:
    UART.print("received: ");
    UART.println(incomingByte, DEC);
    if(incomingByte == ' ') {
      UART.println("new version");
    }
    if(incomingByte == '?') {
      print_port_info();
    }
    #if 0
    if(incomingByte == 'a') {
      ptt_pin = HIGH;
      turn(0, PTT_OFF);
    }
    if(incomingByte == 'A') {
      ptt_pin = LOW;
      turn(0, PTT_ON);
    }
    if(incomingByte == 's') {
      ptt_pin = HIGH;
      start_sequence(ptt_pin == 0);
    }
    if(incomingByte == 'S') {
      ptt_pin = LOW;
      start_sequence(ptt_pin == 0);
    }
    if(incomingByte == 't') {
      events = 1;
      run_timers();
    }
    #endif
  }
#endif

  // millis() - returns some time value as a long
  //delay(1); // time delay of ?
  //delay(time);  dealy(1000) is 1 second
}

// vim: tabstop=2 expandtab shiftwidth=2 softtabstop=2
