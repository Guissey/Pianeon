#ifndef _CONFIG_SERVER_H_
#define _CONFIG_SERVER_H_

#include <WebServer.h>
#include "LedController.h"

#define AP_SSID "Pianeon"
#define AP_PASSWORD "Pianeon"

class ConfigServer {
  public:
    // @param mode 0: inactive, 1: AP, 2: STA
    ConfigServer(LedController* led, uint8_t mode = 0);
    ~ConfigServer();
    void setup();
    void loop();
    
    private:
    WebServer* server;
    LedController* led_controller;
    // 0: inactive, 1: AP, 2: STA
    uint8_t mode;
    void startApMode();
    void startStaMode();
    void onConnect();
    void onPostColor();
    void onPostBrightness();
    void onPostShowSustain();
};

#endif /* _CONFIG_SERVER_H_ */
