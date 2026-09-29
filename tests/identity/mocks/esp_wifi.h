#pragma once
#include <cstdint>
#include "esp_system.h"
typedef int wifi_mode_t;
constexpr wifi_mode_t WIFI_MODE_NULL=0;
esp_err_t esp_wifi_get_mode(wifi_mode_t* mode);
