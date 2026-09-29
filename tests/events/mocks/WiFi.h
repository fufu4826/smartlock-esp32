#pragma once
constexpr int WIFI_STA = 1;
constexpr int WIFI_AP_STA = 2;
class WiFiClass { public: int getMode() const { return WIFI_AP_STA; } };
extern WiFiClass WiFi;
