#ifndef IOBUS_H
#define IOBUS_H

#include <Arduino.h>

/* /////////////////////////////////////////////////////////////////////////////

    IO Data Bus tools

*/ /////////////////////////////////////////////////////////////////////////////

// IOBus is an 8-bit bidirectional bus composed of:
// PORTD[7-2] (pins 7 through 2) and PORTC[1-0] (pins A1 through A0)
// So the mapping is quite neatly [7, 6, 5, 4, 3, 2, 1, 0] => [D7, D6, D5, D4, D3, D2, A1, A0]
// (D stands for Arduino's Digital pin, and A is Arduino's Analog pin)
// This can be modified to use PORTB if Ext. Interrupts on pins 2 and 3 are needed later on. For now, this is optimal.
// Note: This code assumes OUTPUT = 1 and INPUT = 0, as defined by Arduino. Be careful

void iobus_setMode(uint8_t mode){
  if(mode == OUTPUT) {
    // TODO: Add a failsafe. Set addressing lines to a state where we know everyone's quiet (Hi-Z) to prevent "accidents" like bus fights, when switching to OUTPUT mode
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

#endif