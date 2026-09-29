#pragma once

#include <TFT_eSPI.h>
#include "../app/AppState.h"

class DisplayManager {
 public:
  void begin(TFT_eSPI& tft);
  void render(AppState state);
  void backlightOn();
  void backlightOff();

 private:
  TFT_eSPI* tft_ = nullptr;
  static constexpr uint8_t kBacklightPin = 27;
};
