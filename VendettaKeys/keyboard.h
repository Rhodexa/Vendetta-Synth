#ifndef KEYBOARD_H
#define KEYBOARD_H

/* This is the keyboard engine, where the keys are scanned and processed. */

/*
  An entire's sensitive key cycle might look something like this:

             PRESSED   HELD       HALF_RELEASED
             v         v          v                               Timer also counts here, but takes no effect because the Key-ON event has already been issued
              ____________________      RELEASED         ____  v  ___      __      v        and it can only be retriggred after a Key-OFF event, which only
        _____/                    \____ v                __/    \___/   \    /  \    ___       occurs once the key is fully released, not half-released.
  ____/   ^                            \_________ _ ____/   ^         ^  \__/    \__/   \___
  RELEASED  HALF_PRESSED                               KON   No-effect  ^   ^   ^      ^  
  .                                                                       KOFF KON KOFF   No Effect
             KEYON_EVENT              KEYOFF_EVENT                        
             v________________________ v                     ___________      ___
  __________/                         \__________ _ ________/           \____/   \___________

      TIMER COUNT RANGE
       __^__
      /     \  <- TIMER RESET ->   COUNT   RESET           /|     /|                   /|
            _                        v  _    v            / |    / |                  / |
  ____..--'' _____________________..--'' ________ _ _____/  |___/  |_________________/  |____

             ^                           ^
    KEYON VELOCITY SAMPLED      KEYOFF VELOCITY SAMPLED   

  All these stages are very relevant because
  * A Note ON  event can only be triggered once the key traveled from RELEASED to PRESSED, but no other case.
  * A Note OFF event can only be triggered once the key traveled from HELD/PRESSED to RELEASED, and no other case.
  * When a note enters the HALF_PRESSED range, a timer begins counting; This timer is used to compute the 'velocity' property of the note

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
  * 00: RELEASED
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
// Weirdly, it is the keyboard which queues notes for streaming, and not the MIDI engine which polls the keyboard!
// This could be more elegant, but it doesn't have to! :D

struct Key {
  uint8_t state;
  bool keyon;
  uint8_t timer;
};
Key key[N_KEYS];

void keyboard_scan(){
  // Step 1 — Scan all the keys to get their current states
  for (uint8_t i = 0; i < 8; i++) {
    // We need to capture two bytes: 
    // one for the lower part of the keyboard (first 4 keys of the current block) and one for the upper part (last 4 keys of the current block).
    // The following is just the ugly hardcoded nonsense that talks to the button matrix and gets one "block" worth of data,
    // then it stores it in two separate variables _lower and _upper for later decoding and storing.
    uint8_t block_select_mask = 1 << i;

    PORTC = 0b00001001;

    iobus_setMode(OUTPUT);
    iobus_setData(0);
    PORTC = 0b00001011; PORTC = 0b00001001;
    iobus_setData(block_select_mask);
    PORTC = 0b00001101; PORTC = 0b00001001;
    iobus_setMode(INPUT);
    PORTC = 0b00000000;
    delayMicroseconds(16);
    uint8_t column_data_lower = iobus_getData();
    PORTC = 0b00001001;

    iobus_setMode(OUTPUT);
    iobus_setData(0);
    PORTC = 0b00001101; PORTC = 0b00001001;
    iobus_setData(block_select_mask);
    PORTC = 0b00001011; PORTC = 0b00001001;
    iobus_setMode(INPUT);
    PORTC = 0b00000000;
    delayMicroseconds(16);
    uint8_t column_data_upper = iobus_getData();
    PORTC = 0b00001001;

    // Decode and store the 8 key states we just got:
    // * We got 16-bit worth of data
    // * every bit is a button
    // * each key is made up of two buttons
    // * every pair of bits corresponds to a key
    // * the keys in this block are 8 keys appart
    // * if the bit pair is 00 the key is no pressed; if it is 01 or 10, it is half-pressed; if it is 11 it is officially pressed

    // all these weird numbers have to do with the weird way the keyboard pcb is built... im correcting non-linearities by software!
    key[i     ].state = ((column_data_lower     ) & 0b11); // Lower section   
    key[i + 24].state = ((column_data_lower >> 2) & 0b11);                    
    key[i + 16].state = ((column_data_lower >> 4) & 0b11); 
    key[i +  8].state = ((column_data_lower >> 6) & 0b11);  
    key[i + 56].state = ((column_data_upper     ) & 0b11); // Upper section
    key[i + 48].state = ((column_data_upper >> 2) & 0b11);
    key[i + 40].state = ((column_data_upper >> 4) & 0b11); 
    key[i + 32].state = ((column_data_upper >> 6) & 0b11); 
  }
}

void keyboard_findAndQueueKeyEvents(){
  // Step 2 — Process the states to find events and velocity information, then queue any event
  midi_note_queue_index = 0; // flush the midi queue;

  for (uint8_t i = 0; i < N_KEYS; i++) {
    if(key[i].state == 0b11) { // Pressed state
      if (key[i].keyon == false){
        key[i].keyon = true;
        midi_queueKey(true, i, key[i].timer);
      }
      // Order matters! Only reset the timer after checking for events,
      // this gives the event the chance to queue the timer's value before it gets reset
      key[i].timer = 0; 
    }
    else if(key[i].state == 0b00) { // Released state
      if (key[i].keyon == true){
        key[i].keyon = false;
        midi_queueKey(false, i, key[i].timer);
      }
      key[i].timer = 0;
    }
    else {
      // If not RELEASED nor PRESSED we must be in a HALF_PRESSED state!
      // The timer always counts when HALF_PRESS'ed, and gets reset in any other case
      if(key[i].timer < 255) key[i].timer++;
    }
  }
}

#endif