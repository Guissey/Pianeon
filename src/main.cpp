#include <Arduino.h>

// #include "LedController.h"
// #include "ConfigServer.h"
#include "UsbMidiHost.h"

#define PIN_WS2812B 16
#define LED_NUMBER 175
#define USE_PREFERENCES 0
#define WEBSERVER_MODE 2 // 0: inactive, 1: AP, 2: STA

/// Initialisation ///

// LedController led(LED_NUMBER, PIN_WS2812B, USE_PREFERENCES);
// ConfigServer server(&led, WEBSERVER_MODE);
UsbMidiHost usb_midi;

/// Setup ///
void setup() {
  Serial.begin(115200);
  log_d("Start setup");
  // led.setup();
  // server.setup();
  usb_midi.setup();
  log_v("Setup done");
}

/// Loop ///
void loop() {
  // led.blinkLoop();
  // led.show();
  // server.loop();
}
