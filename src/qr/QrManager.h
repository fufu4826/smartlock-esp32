#pragma once

#include <TFT_eSPI.h>

class QrManager {
 public:
  // Renders a bounded text payload as a QR code. The payload is only encoded
  // into the symbol; it is never printed as text or sent to Serial.
  bool render(TFT_eSPI& tft, const char* payload, const char* label,
              uint16_t screenColor = TFT_WHITE);

  static constexpr size_t kMaxPayloadLength = 128;

 private:
  void renderError(TFT_eSPI& tft, const char* message);
};
