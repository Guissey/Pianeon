#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "LedController.h"
#include "secrets.h"

#define PIN_WS2812B 16
#define LED_NUMBER 175
#define USE_PREFERENCES true
#define USE_WEBSERVER true
#define WEBSERVER_MODE "STA" // STA or wifi

/// Declarations ///
LedController led(LED_NUMBER, PIN_WS2812B, USE_PREFERENCES);

/// Setup ///
void setup() {
  Serial.begin(115200);
  led.setup();
  log_v("Setup done");
}

/// Loop ///
void loop() {
  led.blinkLoop();
  led.show();
}
