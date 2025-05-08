#include "LedController.h"

#include <Adafruit_NeoPixel.h>

LedController::LedController(int led_count, int led_strip_pin) {
  ws2812b = new Adafruit_NeoPixel(led_count, led_strip_pin, NEO_GRB + NEO_KHZ800);
  led_number = led_count;
  changes_to_show = false;
}

LedController::~LedController() {
  delete ws2812b;
  // nvs.end();
}

void LedController::setup() {
  // nvs.begin("led_pref", false);
  // ESP_LOGW("", "color %06x, brigthness %d", nvs.getUInt("led_color", DEFAULT_LED_COLOR), nvs.getUChar("brightness", DEFAULT_BRIGHTNESS));
  ws2812b->begin();
  ws2812b->setBrightness(this->getBrightness());
  ws2812b->clear();
  ws2812b->show();
}

void LedController::setColor(uint32_t color) {
  // nvs.putUInt("led_color", color);
  led_color = color;
}

void LedController::setColor(uint8_t red, uint8_t green, uint8_t blue) {
  // nvs.putUInt("led_color", ws2812b->Color(red, green, blue));
  led_color = ws2812b->Color(red, green, blue);
}

uint32_t LedController::getColor() {
  // return nvs.getUInt("led_color", DEFAULT_LED_COLOR);
  return led_color;
}

void LedController::setBrightness(uint8_t brightness) {
  // nvs.putUChar("brightness", brightness);
  this->brightness = brightness;
  ws2812b->setBrightness(brightness);
}

uint8_t LedController::getBrightness() {
  // return nvs.getUChar("brightness", DEFAULT_BRIGHTNESS);
  return brightness;
}

void LedController::setShowSustain(bool show) {
  // nvs.putBool("sustain", showSustain);
  if (!show) {
    this->lightOffSides();
  }
  show_sustain = show;
}

bool LedController::getShowSustain() {
  // return nvs.getBool("sustain", false);
  return show_sustain;
}

void LedController::lightOn(uint8_t note, uint8_t velocity) {
  ws2812b->setPixelColor(this->getLedIndex(note), this->getColor());
  changes_to_show = true;
}

void LedController::lightOff(uint8_t note) {
  ws2812b->setPixelColor(this->getLedIndex(note), 0);
  changes_to_show = true;
}

void LedController::lightOnSides() {
  if (!this->getShowSustain()) return;
  uint32_t color = this->getColor();
  ws2812b->setPixelColor(0, color);
  ws2812b->setPixelColor(led_number - 1, color);
  changes_to_show = true;
}

void LedController::lightOffSides() {
  if (!this->getShowSustain()) return;
  ws2812b->setPixelColor(0, 0);
  ws2812b->setPixelColor(led_number - 1, 0);
  changes_to_show = true;
}

void LedController::show() {
  if (!changes_to_show) return;
  changes_to_show = false;
  ws2812b->show();
}

void LedController::blinkLoop() {
  unsigned long current_millis = millis();
  if (current_millis - last_blink_millis < 300) return;
  last_blink_millis = current_millis;
  this->lightOff(blink_note++);
  if (blink_note >= 35) {
    blink_note = 30;
  }
  this->lightOn(blink_note, 100);
}

int LedController::getLedIndex(uint8_t note) {
  return (note - 21) * 2;
}
