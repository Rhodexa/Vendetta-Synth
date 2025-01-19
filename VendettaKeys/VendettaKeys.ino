/*
  This lib is part of the Vendetta Synth Project.
  This is a private (but open) project. It is not meant to cover anyone's needs directly except for my own personal shenanigans.

  Note this is a hacked-together project coming from a tinkerer, this is not commercial-grade elegance in code or hardware by ANY means!
  It's built to wokr with what I have. :3

  Vendetta's keyboard allows for baudrate changes on demand and on-the-fly... defaults to 57600 on power-on
  But, it supports MIDI-comppliant 31250bauds too, which can be "defaulted" at start-up with a switch!
  57600 is the maximum that can be setup on-the-fly due to display limitations (baudrates and other info are displayed on a screen)
  The display is 3-digits big, so baud rates are displayed in hundred's: Eg. 9600 -> 096   57600 -> 576   19200 -> 192
  Thankfully, most Serial speeds are hundreds-multiples, except for MIDI which is 31250, as you know.

  Sadly Native MIDI and UART can't be used together due to hardware and software limiations of both Vendetta and most operating systems.
  However, it would be "trivial" to create a hardware add-on that would allow for both MIDI and UART to talk together by listening to Vendetta and doing the
  intermediary's role.

  It scans the keyboard at about 60Hz only, so note strokes are quantized to that timeframe. Hopefully that's good enough.
  I reckon 30Hz is already MORE than decent.
*/

/*
  IO Mapping:
  Activity: ------- LED_BUILTIN (13)
  Data Bus: ------- pins 7, 6, 5, 4, 3, 2, A1, A0
  Address Bus: ---- pins 9, 8
  MIDI/Serial: ---- pins 1, 0 (RX, TX)
  MIDI Mode: ------ pin 12
  Mod wheels: ----- A2, A3, A4, A5, A6, A7 (Six analog inputs total)
  Remaining: 11, 10

  Adress lines can be used to multiplex the IO Bus with a display too.
*/


/* /////////////////////////////////////////////////////////////////////////////

    IO Data Bus tools

*/ /////////////////////////////////////////////////////////////////////////////
// IOBus is an 8-bit bidirectional bus composed of:
// PORTD[7-2] (pins 7 through 2) and PORTC[1-0] (pins A1 through A0)
// So the mapping is quite neatly [7, 6, 5, 4, 3, 2, 1, 0] => [D7, D6, D5, D4, D3, D2, A1, A0]
// (D stands for Arduino's Digital pin, and A is Arduino's Analog pin)
enum IOMode {
  OUTPUT,
  INPUT
};

void iobus_setMode(IOMode mode){
  if(mode == IOMode::OUTPUT) {
    DDRD |= 0b11111100;
    DDRC |= 0b00000011;
  }
  else { // default to input, it is safer
    DDRD &= 0b00000011;
    DDRC &= 0b11111100;
  }
}

void iobus_setData(uint8_t data){
  PORTD &= 0b00000011;
  PORTC &= 0b11111100;
  PORTD |= (0b11111100 & data);
  PORTC |= (0b00000011 & data);
}

uint8_t iobus_getData(){
  return (PORTD & 0b11111100) | (PORTC & 0b00000011);
}

/* /////////////////////////////////////////////////////////////////////////////

    My keyboard, my magic, my rules.
    The hardware:

*/ /////////////////////////////////////////////////////////////////////////////

/*
  An entire's sensitive key cycle might look something like this:

             PRESSED   HELD       HALF_RELEASED
             v         v          v     
              ____________________      RELEASED
        _____/                    \____ v
  ____/   ^                            \_____
  IDLE  HALF_PRESS
 
            
             KEYON_EVENT              KEYOFF_EVENT
             v________________________ v
  __________/                         \______


      TIMER COUNT RANGE
       __^__
      /     \  <- TIMER RESET ->   COUNT   RESET            
            _                        v  _    v    
  ____..--'' _____________________..--'' _____

             ^                           ^
    KEYON VELOCITY SAMPLED      KEYOFF VELOCITY SAMPLED   

  All these stages are very relevant because
  * A Note ON  event can only be triggered once the key traveled from IDLE to PRESSED, but no other case.
  * A Note OFF event can only be triggered once the key traveled from HELD/PRESSED to RELEASED, and no other case.
  * When a note enters the HALF_PRESS range, a timer begins counting; This timer is used to compute the 'velocity' property of the note

  Note: Key OFF velocity isn't a standard thing in MIDI stuff, it may or may not work on some setups. I provide a switch for that feature.

*/

enum KeyValues {
  IDLE,
  HALF_PRESS,
  FULL_PRESS
};

struct Key {
  uint8_t state = 0; // Idle for now
};

void setup() {
}

void loop() {

}
