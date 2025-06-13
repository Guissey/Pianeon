#include "LedController.h"

#include <Adafruit_NeoPixel.h>

LedController::LedController(int led_count, int led_strip_pin, bool use_preferences) {
  ws2812b = new Adafruit_NeoPixel(led_count, led_strip_pin, NEO_GRB + NEO_KHZ800);
  led_number = led_count;
  use_nvs = use_preferences;
}

LedController::~LedController() {
  delete ws2812b;
  if (use_nvs) nvs.end();
}

void LedController::setup() {
  if (use_nvs) nvs.begin("Pianeon", false);
  log_i("color %06x, brigthness %d", this->getColor(), this->getBrightness());
  ws2812b->begin();
  ws2812b->setBrightness(this->getBrightness());
  ws2812b->clear();
  ws2812b->show();
}

void LedController::setColor(uint32_t color) {
  if (use_nvs) {
    nvs.putUInt("led_color", color);
  } else {
    led_color_temp = color;
  }
  log_i("led_color set to: %04x", color);
}

void LedController::setColor(uint8_t red, uint8_t green, uint8_t blue) {
  uint32_t color = ws2812b->Color(red, green, blue);
  if (use_nvs) {
    nvs.putUInt("led_color", color);
  } else {
    led_color_temp = color;
  }
  log_i("led_color set to: %04x", color);
}

uint32_t LedController::getColor() {
  return use_nvs
    ? nvs.getUInt("led_color", DEFAULT_LED_COLOR)
    : led_color_temp;
}

void LedController::setBrightness(uint8_t brightness) {
  if (use_nvs) {
    nvs.putUChar("brightness", brightness);
  } else {
    brightness_temp = brightness;
  }
  ws2812b->setBrightness(brightness);
  log_i("brightness set to: %d", brightness);
}

uint8_t LedController::getBrightness() {
  return use_nvs
    ? nvs.getUChar("brightness", DEFAULT_BRIGHTNESS)
    : brightness_temp;
}

void LedController::setShowSustain(bool showSustain) {
  if (!showSustain) this->lightOffSides();
  if (use_nvs) {
    nvs.putBool("sustain", showSustain);
  } else {
    show_sustain_temp = showSustain;
  }
  log_i("show_sustain set to: %d", showSustain);
}

bool LedController::getShowSustain() {
  return use_nvs
    ? nvs.getBool("sustain", false)
    : show_sustain_temp;
}

void LedController::lightOn(uint8_t note, uint8_t velocity) {
  ws2812b->setPixelColor(this->computePixelIndex(note), this->getColor());
  // changes_to_show = true;
  ws2812b->show();
}

void LedController::lightOff(uint8_t note) {
  ws2812b->setPixelColor(this->computePixelIndex(note), 0);
  // changes_to_show = true;
  ws2812b->show();
}

void LedController::lightOnSides() {
  if (!this->getShowSustain()) return;
  uint32_t color = this->getColor();
  ws2812b->setPixelColor(0, color);
  ws2812b->setPixelColor(led_number - 1, color);
  // changes_to_show = true;
  ws2812b->show();
}

void LedController::lightOffSides() {
  if (!this->getShowSustain()) return;
  ws2812b->setPixelColor(0, 0);
  ws2812b->setPixelColor(led_number - 1, 0);
  // changes_to_show = true;
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

int LedController::computePixelIndex(uint8_t note) {
  return (note - 21) * 2;
}
