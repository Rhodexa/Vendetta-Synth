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

  A bit about our keyboard's hardware:
  Turns out it uses a 16 x 8 matrix:
   8-bit Add = 1   8-bit Add = 2
  [    HC 373   ] [    HC 373   ]      Add = 4
  | | | | | | | | | | | | | | | |      ___
  + + + + + + + + + + + + + + + + --> |   | KEY0 A \ Key 0
  + + + + + + + + + + + + + + + + --> | H | KEY0 B / BA / State Bits
  + + + + + + + + + + + + + + + + --> | C | KEY1 A \ Key 1
  + + + + + + + + + + + + + + + + --> |   | KEY1 B / BA / State Bits
  + + + + + + + + + + + + + + + + --> | 2 | KEY2 A \ Key 2
  + + + + + + + + + + + + + + + + --> | 4 | KEY2 B / BA / State Bits
  + + + + + + + + + + + + + + + + --> | 5 | KEY3 A \ Key 3
  + + + + + + + + + + + + + + + + --> |___| KEY3 B / BA / State Bits

  BA -> State bits
  * 00: IDLE
  * 01: Half-press
  * 10: Shouldn't happen but it is still half-press
  * 11: Full press

  Keys are read in groups of four per byte, where the value or state of the key is encoded as two bits
  [Key i*4 + 3][Key i*4 + 2][Key i*4 + 1][Key i*4 + 0]

  The HC373 work as column selectors. Basically, to read column n, the value 1 << n should be written to the corresponding buffer.
  For n > 7, the second buffer should be used.
  Once the value is written to the flipflops, the HC245 should be selected for readout of the entire colum at once.

  Note: Yes, the HC373 has an /OE signal, so -in theory- the HC245 shouldn't be needed. However, other things may communicate over the data buffer,
  this means some non-keyboard-related signals could backfeed through the buttons if there's nothing to block the terminals out of the bus.
  Hence, the HC245 transeiver IS needed, but only in one direction.
*/

// It's actually 122 in hardware,
// but because it is a 16x8 button matrix, there's actually 128 possible buttons, 6 of which are essentially ghosts.
// Assuming they exist anyway makes the code resposible for scanning the keyboard much simpler
// It's only six missed buttons anyways... this equates to three keys, so it's not a huge loss, only about 4.6% inefficient?
// I mean, they are still there, you just need to add in the hardware for them. Free extra six buttons if you wanna go _haywire_, pun forcefully intended.
const uint8_t N_KEYS = 128;

enum KeyValues {
  IDLE,
  HALF_PRESS,
  FULL_PRESS
};

struct Key {
  uint8_t state = 0; // Idle for now
};

Key keys[N_KEYS];

void keyboard_scan(){
  for (uint8_t i = 0; i < N_KEYS; i++) {
    keys[i].state = 
  }
}

void setup() {
}

void loop() {

}
