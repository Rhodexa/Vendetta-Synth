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
  However, I use oversampling to get decent dynamic range when it comes to the velocity. Because a timer has to be used, I need the timer to count and sample fast enought o get a 
  decent range.
  So maybe 120 to 240Hz if the ATmega328 can manage!
  That gives us 1 second of travel for the gentle-est key_is_on at 120Hz.
  Or 0.5s at 240Hz
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

#include "iobus.h"
#include "midi.h"
#include "keyboard.h"

void setup() {
  PORTB = 0b00000000;
}

auto current_millis = millis();
auto last_scan = current_millis;
uint8_t time_divider = 0;
void loop() {
  if(current_millis - last_scan >= 8) {
    last_scan = current_millis;
    keyboard_scanAndQueue();
    midi_reshapeQueue();
    midi_sendQueue();
  }
}
