/*
  This lib is part of the Vendetta Synth Project.
  This is a private (but open) project. It is not meant to cover anyone's needs directly except for my own personal shenanigans.

  Note this is a hacked-together project coming from a tinkerer, this is not commercial-grade elegance in code or hardware by ANY means!
  It's built to wokr with what I have. :3

  Based on the MIDI 1.0 Specification: https://drive.google.com/file/d/1ewRrvMEFRPlKon6nfSCxqnTMEu70sz0c/view

  Vendetta's keyboard allows for baudrate changes on demand and on-the-fly... defaults to 57600 on power-on
  But, it supports MIDI-comppliant 31250bauds too, which can be "defaulted" at start-up with a switch!
  57600 is the maximum that can be setup on-the-fly due to display limitations (baudrates and other info are displayed on a screen)
  The display is 3-digits big, so baud rates are displayed in hundred's: Eg. 9600 -> 096   57600 -> 576   19200 -> 192
  Thankfully, most Serial speeds are hundreds-multiples, except for MIDI which is 31250, as you know.

  Sadly Native MIDI and UART can't be used together due to hardware and software limiations of both Vendetta and most operating systems.
  However, it would be "trivial" to create a hardware add-on that would allow for both MIDI and UART to talk together by listening to Vendetta and doing the
  intermediary's role.
  *** Edit: Turns out UART does sometimes support 31250baud in some systems. If your USB-UART adapter supports it, you might actually be able to send Native-MIDI
  and standard UART together!
  
*/

/*
  IO Mapping:
  Activity: ------- LED_BUILTIN (13)
  Data Bus: ------- pins 11, 10, 9, 8, 7, 6, 5, 4
  Address Bus: ---- (undefine yet)
  MIDI/Serial: ---- pins 1, 0 (RX, TX)
  Mod wheels: ----- A6, A7
  Modulators: ----- A5, A4, A3, A2
  Free pins:
  13, 12, A1, A0, 3, 2

  Adress lines can be used to multiplex the IO Bus with a display too.
*/

#include "iobus.h"
#include "midi.h"
#include "keyboard.h"

void setup() {
  DDRC = 0b00001111;
  PORTC = 0b00001111;
  Serial.begin(57600);
  Serial.println("ready.");
  pinMode(13, OUTPUT);
}

auto current_millis = millis();
auto last_scan = current_millis;
void loop() {
  current_millis = millis();
  if(current_millis - last_scan >= 4) {
    last_scan = current_millis;
    keyboard_scan();
    keyboard_findAndQueueKeyEvents();
    midi_reshapeQueue();
    midi_sendQueue();
  }
  if(midi_note_queue_index != 0) digitalWrite(13, 1);
  else digitalWrite(13, 0);
}

void debugKeys(){
  for(int i = 0; i < N_KEYS; i++){
    uint8_t s = key[i].state;
    if(s == 0) Serial.print(" ");       // Not pressed
    else if(s == 3) Serial.print("X");  // Fully Pressed
    else Serial.print(".");             // Half pressed
  }
  Serial.print("|\n");
}