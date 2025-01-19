#ifndef MIDI_H
#define MIDI_H

#include <Arduino.h>
#include "hardware.h"

/* This is the MIDI engine, the core of the keyboard interpreter. */

// To prevent sending notes apart from eachother do to computation overhead,
// we queue key events first to send them all together later.

struct MIDINoteEvent {
  bool is_note_on;
  uint8_t pitch;
  uint8_t velocity;
};
MIDINoteEvent midi_note_queue[N_KEYS]; 
uint8_t midi_note_queue_index = 0;

void midi_queueKey(bool is_note_on, uint8_t pitch, uint8_t velocity){
  midi_note_queue[midi_note_queue_index].is_note_on = is_note_on;
  midi_note_queue[midi_note_queue_index].pitch = pitch;
  midi_note_queue[midi_note_queue_index].velocity = velocity;
  midi_note_queue_index++;
}

struct MIDIProperties{
  // This sets the note that will be sent when you press the first (lowest) key on your board.
  // Defaults C2 for my 5-octave 61-key setup
  int pitch_offset = 36;

  // This can be used to offset the keys either in pitch values (semitones) or entire octaves.
  int octave_offset = 0;
  int transpose = 0;

  // Allows you to send the velocity at wich the key was _released_. Some synths may do cool things with this, others may get confused!
  bool send_note_off_velocity = false;

  // Some systems may expect a Note ON event with 0 velocity rather than the standard Key OFF event! (Weird)
  bool send_note_on_with_zero_velocity = false;
} midishape;

// The queue is built by the keyboard scanner, but those events are not valid MIDI yet,
// we are gonna transform them before we actually send them
void midi_reshapeQueue(){
  for(uint8_t i = 0; i < midi_note_queue_index; i++) {
    // first, we are gonna convert absolute key indexes into MIDI pitch values and apply other goodies like transposes
    uint8_t pitch = midi_note_queue[midi_note_queue_index].pitch;
    pitch = pitch
    + midishape.pitch_offset
    + midishape.transpose
    + (midishape.octave_offset * 12);
    midi_note_queue[midi_note_queue_index].pitch = pitch;

    // now we need to rescale the velocity, since what we get from the scaner is actually how long it took for the key to go from one state to another,
    // we need to convert that into MIDI-compliant velocity
    uint8_t velocity = midi_note_queue[midi_note_queue_index].velocity;
    velocity = (255-velocity)/2; // for now, we are just gonna map it directly from 0 to 127.
    midi_note_queue[midi_note_queue_index].velocity = velocity;
  }
}

void midi_sendQueue(){
  for(uint8_t i = 0; i < midi_note_queue_index; i++) {
    if(midi_note_queue[midi_note_queue_index].is_note_on){
      /*midi_sendNoteOn(
        midi_note_queue[midi_note_queue_index].pitch,
        midi_note_queue[midi_note_queue_index].velocity,
        1
      );*/
    }
  }
}

#endif