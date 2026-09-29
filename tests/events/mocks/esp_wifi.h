#pragma once

using wifi_mode_t = int;
constexpr wifi_mode_t WIFI_MODE_NULL = 0;
constexpr int ESP_OK = 0;
constexpr int ESP_FAIL = -1;

inline int esp_wifi_get_mode(wifi_mode_t*) { return ESP_FAIL; }
