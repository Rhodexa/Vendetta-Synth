#ifndef IOBUS_H
#define IOBUS_H

#include <Arduino.h>

/* /////////////////////////////////////////////////////////////////////////////

    IO Data Bus tools

*/ /////////////////////////////////////////////////////////////////////////////

// IOBus is an 8-bit bidirectional bus composed of:
// PORTD[7-4] (pins 7 through 4) and PORTB[3-0] (pins 11 through 8)
// So the mapping is quite neatly [7, 6, 5, 4, 3, 2, 1, 0] => [D11, D10, D9, D8, D7, D6, D5, D4]
// This makes it very easy to use a simple SIL/header connector directly on the Arduino Pro Mini / Nano to plug the 8-bit bus.
// It also leaves pins 0, 1, 2 and 3 free for serial and external interrupts if needed, which is neat.
// Note: This code assumes OUTPUT = 1 and INPUT = 0, as defined by Arduino. Be careful

// Because the header in the Arduino Pro Mini / Nano exposes the bus with the nibbles physically swapped ([3210][7654]),
// we can correct that in code for easier connections, you can disable this if you rather correct it in hardware for whatever reason.
const bool IOBUS_ENABLE_SWAP_NIBBLES = true;

// I hope the compiler will get this as a hint to use the SWAP NIBBLES instruction available in AVR... compilers are pretty clever (much more than you think!)
inline uint8_t swapNibbles(uint8_t byte) {
  return (byte >> 4) | (byte << 4);
}

void iobus_setMode(uint8_t mode){  
  if(mode == OUTPUT) {
    // TODO: Add a failsafe. Set addressing lines to a state where we know everyone's quiet (Hi-Z) to prevent "accidents" like bus fights, when switching to OUTPUT mode
    DDRD |= 0b11110000;
    DDRB |= 0b00001111;
  }
  else { // default to input, it is safer
    DDRD &= 0b00001111;
    DDRB &= 0b11110000;
  }
}

void iobus_setData(uint8_t data){
  if(IOBUS_ENABLE_SWAP_NIBBLES) data = swapNibbles(data);
  PORTD = (PORTD & 0b00001111) | (data & 0b11110000); 
  PORTB = (PORTB & 0b11110000) | (data & 0b00001111);
}

uint8_t iobus_getData(){
  uint8_t data = (PIND & 0b11110000) | (PINB & 0b00001111);
  if(IOBUS_ENABLE_SWAP_NIBBLES) data = swapNibbles(data);
  return data;
}

#endif