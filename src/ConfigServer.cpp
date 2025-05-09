#include "ConfigServer.h"

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <string>

#include "LedController.h"
#include "secrets.h"
#include "index.h"

ConfigServer::ConfigServer(LedController* led, uint8_t webserver_mode) {
  mode = webserver_mode;
  led_controller = led;
  server = new WebServer(80);
}

ConfigServer::~ConfigServer() {
  delete server;
}

void ConfigServer::setup() {
  if (mode == 1) this->startApMode();
  else if (mode == 2) this->startStaMode();
}

void ConfigServer::loop() {
  if (mode) server->handleClient();
}

void ConfigServer::onConnect() {
  uint8_t brightness = led_controller->getBrightness();
  uint32_t led_color = led_controller->getColor();
  bool sustain = led_controller->getShowSustain();
  char brightnessString[4], redString[4], greenString[4], blueString[4];
  sprintf(brightnessString, "%d", brightness);
  sprintf(redString, "%d", ((uint8_t*)&led_color)[2]);
  sprintf(greenString, "%d", ((uint8_t*)&led_color)[1]);
  sprintf(blueString, "%d", ((uint8_t*)&led_color)[0]);
  server->send(200, "text/html", getHtmlPage(brightnessString, redString, greenString, blueString, sustain).c_str());
}

void ConfigServer::startApMode() {
  log_i("Starting AP Mode");
  IPAddress local_ip(LOCAL_IP);
  IPAddress gateway(GATEWAY_IP);
  IPAddress subnet(SUBNET);
  WiFi.softAP(AP_SSID, NULL);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  delay(100);

  server->on("/", [this](){ this->onConnect(); });
  server->on("/color", HTTP_POST, [this](){ this->onPostColor(); });
  server->on("/brightness", HTTP_POST, [this](){ this->onPostBrightness(); });
  server->on("/sustain", HTTP_POST, [this](){ this->onPostShowSustain(); });
  server->begin();
}

void ConfigServer::startStaMode() {
  log_i( "Connecting to wifi network %s with password %s", WIFI_SSID, WIFI_PASSWORD);
  // WiFi.config(IPAddress(192,168,1,200), IPAddress(192,168,1,1), IPAddress(255,255,255,0));
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.waitForConnectResult(10000);
  if (WiFi.status() == WL_CONNECTED) {
    log_i("Connected with IP %s", WiFi.localIP().toString().c_str());
    server->on("/", [this](){ this->onConnect(); });
    server->on("/color", HTTP_POST, [this](){ this->onPostColor(); });
    server->on("/brightness", HTTP_POST, [this](){ this->onPostBrightness(); });
    server->on("/sustain", HTTP_POST, [this](){ this->onPostShowSustain(); });
    server->begin();
  } else {
    log_e("Connection failed with status %d", WiFi.status());
    WiFi.disconnect();
    this->startApMode();
  }
}

void ConfigServer::onPostColor() {
  if (!server->hasArg("plain")) {
    server->send(400, "application/json", R"({ "error": "missing body" })");
    return;
  }
  JsonDocument json;
  deserializeJson(json, server->arg("plain"));

  led_controller->setColor(json["red"], json["green"], json["blue"]);
  server->send(200, "application/json", R"({ "status": "ok" })");
}

void ConfigServer::onPostBrightness() {
  if (!server->hasArg("plain")) {
    server->send(400, "application/json", R"({ "error": "missing body" })");
    return;
  }
  JsonDocument json;
  deserializeJson(json, server->arg("plain"));

  led_controller->setBrightness(json["brightness"]);
  server->send(200, "application/json", R"({ "status": "ok" })");
}

void ConfigServer::onPostShowSustain() {
  if (!server->hasArg("plain")) {
    server->send(400, "application/json", R"({ "error": "missing body" })");
    return;
  }
  JsonDocument json;
  deserializeJson(json, server->arg("plain"));

  led_controller->setShowSustain(json["sustain"] == true);
  server->send(200, "application/json", R"({ "status": "ok" })");
}
