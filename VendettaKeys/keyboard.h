#ifndef KEYBOARD_H
#define KEYBOARD_H

/* This is the keyboard engine, where the keys are scanned and processed. */

/*
  An entire's sensitive key cycle might look something like this:

             PRESSED   HELD       HALF_RELEASED
             v         v          v     
              ____________________      RELEASED
        _____/                    \____ v
  ____/   ^                            \_____
  IDLE  HALF_PRESS
 
            
             key_is_on_EVENT              KEYOFF_EVENT
             v________________________ v
  __________/                         \______


      TIMER COUNT RANGE
       __^__
      /     \  <- TIMER RESET ->   COUNT   RESET            
            _                        v  _    v    
  ____..--'' _____________________..--'' _____

             ^                           ^
    key_is_on VELOCITY SAMPLED      KEYOFF VELOCITY SAMPLED   

  All these stages are very relevant because
  * A Note ON  event can only be triggered once the key traveled from IDLE to PRESSED, but no other case.
  * A Note OFF event can only be triggered once the key traveled from HELD/PRESSED to RELEASED, and no other case.
  * When a note enters the HALF_PRESS range, a timer begins counting; This timer is used to compute the 'velocity' property of the note

  Note: Key OFF velocity isn't a standard thing in MIDI stuff, it may or may not work on some setups. I provide a switch for that feature.

  A bit about our keyboard's hardware:  
  Turns out it uses a 16 x 8 matrix:

    Addr Bit 0      Addr Bit 1      Addr Bit 2       An HC136 could be used to command this using two lines: 00: Hi-Z - 01: Row Block 0 - 10: Row Block 2 - 11: Read back
      (Latch)         (Latch)         (/OE)          although... if it is a matter of saving ONE pin, I wouldn't worry much. If I get a HC138 instead, then things
         v               v              v            become a bit juicier.
   8-bit Add = 1   8-bit Add = 2
  [    HC 373   ] [    HC 373   ]      Add = 4
  | | | | | | | | | | | | | | | |      ___
  + + + + + + + + + + + + + + + + --> |   | KEY0 A \ Key 0
  + + + + + + + + + + + + + + + + --> | H | KEY0 B / BA / State Bits
  + + + + + + + + + + + + + + + + --> | C | KEY1 A \ Key 16
  + + + + + + + + + + + + + + + + --> |   | KEY1 B / BA / State Bits
  + + + + + + + + + + + + + + + + --> | 2 | KEY2 A \ Key 32
  + + + + + + + + + + + + + + + + --> | 4 | KEY2 B / BA / State Bits
  + + + + + + + + + + + + + + + + --> | 5 | KEY3 A \ Key 48
  + + + + + + + + + + + + + + + + --> |___| KEY3 B / BA / State Bits

  BA -> State bits
  * 00: IDLE
  * 01: Half-press
  * 10: Shouldn't happen but it is still half-press
  * 11: Full press

  Keys are read in groups of four per byte, where the value or state of the key is encoded as two bits
  [Key i + 48][Key i + 32][Key i + 16][Key i + 0]

  The HC373 work as column selectors. Basically, to read column n, the value 1 << n should be written to the corresponding buffer.
  For n > 7, the second buffer should be used.
  Once the value is written to the flipflops, the HC245 should be selected for readout of the entire colum at once.

  Note: Yes, the HC373 has an /OE signal, so -in theory- the HC245 shouldn't be needed. However, other things may communicate over the data buffer,
  this means some non-keyboard-related signals could backfeed through the buttons if there's nothing to block the terminals out of the bus.
  Hence, the HC245 transeiver IS needed, but only in one direction.
*/


#include <Arduino.h>
#include "hardware.h"
#include "iobus.h"
#include "midi.h"


enum KeyStates{
  IDLE,
  HALF_PRESS,
  FULL_PRESS
};

struct Key {
  uint8_t state;
  bool key_is_on;
  uint8_t timer;
};
Key key[N_KEYS];

void keyboard_scanAndQueue(){
  midi_note_queue_index = 0; // flush the send queue;

  // Scan all the keys to get their current states
  for (uint8_t i = 0; i < N_COLUMNS; i++) {
    if (i < 8) {
      // select block 0
    }
    else { // In theory, the the loop should be skipped right over if we go overange... but _I_ know that _I_ don't need that!
      // select block 1
    }
    iobus_setMode(INPUT);
    uint8_t column_data = iobus_getData();
    key[i     ].state = ((column_data & 0x01) != 0) + ((column_data & 0x02) != 0); // Extract bits 0 and 1, then add them together. Yes, this makes sense.
    key[i + 16].state = ((column_data & 0x04) != 0) + ((column_data & 0x08) != 0); // Now 0 = IDLE, 1 = HALF_PRESS, and 2 = FULL_PRESS
    key[i + 32].state = ((column_data & 0x10) != 0) + ((column_data & 0x20) != 0); 
    key[i + 48].state = ((column_data & 0x40) != 0) + ((column_data & 0x80) != 0); 
  }

  // Process the states to get events and velocity information, then queue any event
  for (uint8_t i = 0; i < N_KEYS; i++) {
    // Key logic
    if((key[i].state == KeyStates::FULL_PRESS) && (key[i].key_is_on == false)){
        key[i].key_is_on = true;
        midi_queueKey(true, i, key[i].timer);
    }
    else if((key[i].state == KeyStates::IDLE) && (key[i].key_is_on == true)) {
        key[i].key_is_on = false;
        midi_queueKey(false, i, key[i].timer);
    }

    // Timer section... the timer always counts when half_pressed, and gets reset in any other case
    if((key[i].state == KeyStates::HALF_PRESS) && (key[i].timer < 255)) key[i].timer++;
    else key[i].timer = 0;
  }
}

#endif