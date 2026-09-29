#pragma once

#include "Arduino.h"
#include "IPAddress.h"

constexpr int WIFI_AP = 1;
constexpr int WIFI_STA = 2;
constexpr int WIFI_AP_STA = 3;
constexpr int WIFI_SCAN_RUNNING = -1;
constexpr int WIFI_AUTH_OPEN = 0;
constexpr int WL_DISCONNECTED = 0;
constexpr int WL_CONNECTED = 3;

struct WiFiMock {
  bool persistentValue = true;
  bool sleepEnabled = true;
  int currentMode = 0;
  int currentStatus = WL_DISCONNECTED;
  IPAddress apAddress;
  IPAddress gateway;
  IPAddress netmask;
  IPAddress stationAddress;
  bool apActive = false;
  bool softApShouldFail = false;
  uint8_t apStationCount = 0;
  char activeApSsid[33] = {};
  char activeApPassword[65] = {};
  unsigned softApCalls = 0;
  unsigned disconnectCalls = 0;
  unsigned beginCalls = 0;
  char lastBeginSsid[33] = {};
  char lastBeginPassword[65] = {};

  void reset();
  void persistent(bool value) { persistentValue = value; }
  bool setSleep(bool value) { sleepEnabled = value; return true; }
  bool setHostname(const char*) { return true; }
  bool mode(int value) { currentMode = value; if (value == WIFI_STA) apActive = false; return true; }
  bool softAPConfig(IPAddress address, IPAddress gw, IPAddress mask) {
    apAddress = address;
    gateway = gw;
    netmask = mask;
    return true;
  }
  bool softAP(const char* ssid, const char* password = nullptr) {
    if (softApShouldFail) return false;
    apActive = true;
    ++softApCalls;
    std::snprintf(activeApSsid, sizeof(activeApSsid), "%s", ssid ? ssid : "");
    std::snprintf(activeApPassword, sizeof(activeApPassword), "%s",
                  password ? password : "");
    return true;
  }
  void softAPsetHostname(const char*) {}
  IPAddress softAPIP() const { return apActive ? apAddress : IPAddress(); }
  uint8_t softAPgetStationNum() const { return apStationCount; }
  void disconnect(bool) { ++disconnectCalls; }
  void begin(const char* ssid, const char* password) {
    ++beginCalls;
    std::snprintf(lastBeginSsid, sizeof(lastBeginSsid), "%s", ssid);
    std::snprintf(lastBeginPassword, sizeof(lastBeginPassword), "%s", password);
  }
  int status() const { return currentStatus; }
  IPAddress localIP() const { return stationAddress; }
  int32_t RSSI() const { return -55; }
  int scanComplete() const { return 0; }
  void scanDelete() {}
  int scanNetworks(bool, bool) { return 0; }
  String SSID(int) const { return String(); }
  int32_t RSSI(int) const { return -55; }
  int encryptionType(int) const { return WIFI_AUTH_OPEN; }
};

extern WiFiMock WiFi;
