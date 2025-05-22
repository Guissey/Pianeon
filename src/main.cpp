#include <Arduino.h>

// #include "LedController.h"
// #include "ConfigServer.h"
#include "UsbMidiHost.h"
// #include "usb_midi_host.hpp"

#define PIN_WS2812B 16
#define LED_NUMBER 175
#define USE_PREFERENCES 0
#define WEBSERVER_MODE 0 // 0: inactive, 1: AP, 2: STA

/// Initialisation ///

// LedController led(LED_NUMBER, PIN_WS2812B, USE_PREFERENCES);
// ConfigServer server(&led, WEBSERVER_MODE);
UsbMidiHost usb_midi;

void midiInCallbackMain(midi_usb_packet packet) {
  if (packet.midi_type == 0x08 || packet.midi_type == 0x09) {
    ESP_LOGI(
      "",
      "Main callback midi packet: cable number: %02x, code index: %02x, midi channel: %02x, midi type: %02x, data1: %02x, data2: %02x",
      packet.usb_cable_number, packet.code_index_number, packet.midi_channel, packet.midi_type, packet.midi_data_1, packet.midi_data_2
    );
  }
}

/// Setup ///
void setup() {
  Serial.begin(115200);
  log_d("Start setup");
  // led.setup();
  // server.setup();
  usb_midi.setMidiInCallback(&midiInCallbackMain);
  usb_midi.setup();
  // setupUsbMidi();
}

/// Loop ///
void loop() {
  // led.blinkLoop();
  // led.show();
  // server.loop();
  // loopUsbMidi();
}
