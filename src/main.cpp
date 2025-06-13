#include <Arduino.h>

#include "LedController.h"
#include "ConfigServer.h"
#include "UsbMidiHost.h"

#define PIN_WS2812B 16
#define LED_NUMBER 175
#define USE_PREFERENCES 1
#define WEBSERVER_MODE WIFI_MODE_STA // 0: inactive, WIFI_MODE_STA or WIFI_MODE_AP

/// Functions declaration ///

void midiInCallbackMain(midi_usb_packet packet);

/// Variables ///

LedController led(LED_NUMBER, PIN_WS2812B, USE_PREFERENCES);
ConfigServer server(&led, WEBSERVER_MODE);
UsbMidiHost usb_midi;

/// Setup ///
void setup() {
  Serial.begin(115200);
  log_d("Start setup");
  led.setup();
  server.setup();
  usb_midi.setMidiInCallback(&midiInCallbackMain);
  usb_midi.setup();
}

/// Loop ///
void loop() {
  server.loop();
  // led.blinkLoop();
}

/// Functions definition ///

void midiInCallbackMain(midi_usb_packet packet) {
  if ( // Note Off
    packet.midi_type == MIDI_NOTE_OFF 
    || (packet.midi_type == MIDI_NOTE_ON && packet.midi_data_2 == 0)
  ) {
    log_d("Note OFF: %d", packet.midi_data_1);
    led.lightOff(packet.midi_data_1);
  } else if (packet.midi_type == MIDI_NOTE_ON) { // Note On
    log_d("Note ON : %d, velocity: %d", packet.midi_data_1, packet.midi_data_2);
    led.lightOn(packet.midi_data_1, packet.midi_data_2);
  } else if (packet.midi_type == 0x0b && packet.midi_data_1 == 0x40) { // Sustain
    if (packet.midi_data_2 < 64) {
      log_d("Sustain OFF");
      led.lightOffSides();
    } else {
      log_d("Sustain ON");
      led.lightOnSides();
    }
  }
}