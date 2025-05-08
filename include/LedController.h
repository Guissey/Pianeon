#ifndef _LED_CONTROLLER_H_
#define _LED_CONTROLLER_H_

#define DEFAULT_LED_COLOR 0xffffff
#define DEFAULT_BRIGHTNESS 100

#include <Adafruit_NeoPixel.h>
// #include <Preferences.h>
// #include <elapsedMillis.h>

class LedController {
  public:
    LedController(int led_count, int led_strip_pin);
    ~LedController();
    void setup();
    void setColor(uint32_t);
    void setColor(uint8_t red, uint8_t green, uint8_t blue);
    uint32_t getColor();
    void setBrightness(uint8_t);
    uint8_t getBrightness();
    void setShowSustain(bool);
    bool getShowSustain();
    void lightOn(uint8_t note, uint8_t velocity);
    void lightOff(uint8_t note);
    void lightOnSides();
    void lightOffSides();
    void show();
    void blinkLoop();

  private:
    Adafruit_NeoPixel* ws2812b;
    // Preferences nvs;
    uint16_t led_number;
    uint32_t led_color = 0xFFFFFF;
    uint8_t brightness = 100;
    bool show_sustain = true;
    bool changes_to_show;
    int getLedIndex(uint8_t note);
    int blink_note = 30;
    unsigned long last_blink_millis = 0;
};

#endif /* _LED_CONTROLLER_H_ */
