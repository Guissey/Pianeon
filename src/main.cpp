#define PIN_WS2812B 16
#define LED_NUMBER 175

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "LedController.h"

/// Declarations ///
LedController led(LED_NUMBER, PIN_WS2812B);

/// Setup ///
void setup() {
  Serial.begin(115200);
  led.setup();
}

/// Loop ///
void loop() {
  led.blinkLoop();
  led.show();
}

/// Definitions ///