#pragma once
using esp_err_t = int;
enum wifi_mode_t { WIFI_MODE_NULL, WIFI_MODE_STA, WIFI_MODE_AP, WIFI_MODE_APSTA };
constexpr esp_err_t ESP_OK = 0;
inline esp_err_t esp_wifi_get_mode(wifi_mode_t* mode) { if (mode) *mode=WIFI_MODE_STA; return ESP_OK; }
