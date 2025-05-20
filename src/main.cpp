#include <Arduino.h>

#include "LedController.h"
#include "ConfigServer.h"

#define PIN_WS2812B 16
#define LED_NUMBER 175
#define USE_PREFERENCES 0
#define WEBSERVER_MODE 1 // 0: inactive, 1: AP, 2: STA

/// Initialisation ///

LedController led(LED_NUMBER, PIN_WS2812B, USE_PREFERENCES);
ConfigServer server(&led, WEBSERVER_MODE);

/// Setup ///
void setup() {
  Serial.begin(115200);
  led.setup();
  server.setup();
  log_v("Setup done");
}

/// Loop ///
void loop() {
  led.blinkLoop();
  led.show();
  server.loop();
}
