#include "QrManager.h"

#include <QRCode.h>
#include <cstring>

namespace {
constexpr uint8_t kQrVersion = 10;
constexpr uint8_t kQuietZoneModules = 4;
constexpr uint16_t kQrErrorCorrection = ECC_LOW;
constexpr size_t kQrBufferCapacity = 700;
// Font 4 is enabled in this firmware and keeps a short status label readable.
// Sixteen bytes leaves room for the longest expected ASCII label on 320 px.
constexpr size_t kMaxLabelLength = 16;
}

void QrManager::renderError(TFT_eSPI& tft, const char* message) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("QR ERROR", tft.width() / 2, tft.height() / 2 - 16, 4);
  tft.drawString(message, tft.width() / 2, tft.height() / 2 + 24, 4);
}

bool QrManager::render(TFT_eSPI& tft, const char* payload, const char* label,
                       uint16_t screenColor) {
  if (payload == nullptr || payload[0] == '\0') {
    renderError(tft, "Empty payload");
    return false;
  }

  const size_t payloadLength = strnlen(payload, kMaxPayloadLength + 1);
  if (payloadLength > kMaxPayloadLength) {
    renderError(tft, "Payload too long");
    return false;
  }

  uint8_t qrBuffer[kQrBufferCapacity] = {};
  QRCode qrCode;
  if (qrcode_getBufferSize(kQrVersion) > sizeof(qrBuffer) ||
      qrcode_initText(&qrCode, qrBuffer, kQrVersion, kQrErrorCorrection, payload) < 0) {
    renderError(tft, "Encoding failed");
    return false;
  }

  const int displayWidth = tft.width();
  const int displayHeight = tft.height();
  const int totalModules = qrCode.size + 2 * kQuietZoneModules;
  const int quietSizeLimit = min(displayWidth, displayHeight - 64) / totalModules;
  if (qrCode.size <= 0 || quietSizeLimit < 1) {
    renderError(tft, "Display too small");
    return false;
  }

  const int moduleScale = min(quietSizeLimit, 4);
  const int symbolPixels = qrCode.size * moduleScale;
  const int quietPixels = kQuietZoneModules * moduleScale;
  const int totalPixels = symbolPixels + 2 * quietPixels;
  const int left = (displayWidth - totalPixels) / 2;
  const int labelAreaHeight = label == nullptr || label[0] == '\0' ? 0 : 48;
  const int availableTop = labelAreaHeight == 0 ? 0 : labelAreaHeight;
  const int top = availableTop + (displayHeight - availableTop - totalPixels) / 2;

  // Keep the QR symbol and its full quiet zone white regardless of the page
  // color. Black modules on white retain the optical contrast phones expect.
  tft.fillScreen(screenColor);
  tft.fillRect(left, top, totalPixels, totalPixels, TFT_WHITE);
  if (labelAreaHeight != 0) {
    char boundedLabel[kMaxLabelLength + 1];
    size_t labelLength = strnlen(label, kMaxLabelLength);
    memcpy(boundedLabel, label, labelLength);
    boundedLabel[labelLength] = '\0';
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor((screenColor == TFT_RED || screenColor == TFT_BLUE)
                         ? TFT_WHITE : TFT_BLACK,
                     screenColor);
    tft.drawString(boundedLabel, displayWidth / 2, 24, 4);
  }

  const int matrixLeft = left + quietPixels;
  const int matrixTop = top + quietPixels;
  for (uint8_t y = 0; y < qrCode.size; ++y) {
    for (uint8_t x = 0; x < qrCode.size; ++x) {
      if (qrcode_getModule(&qrCode, x, y)) {
        tft.fillRect(matrixLeft + x * moduleScale,
                     matrixTop + y * moduleScale,
                     moduleScale, moduleScale, TFT_BLACK);
      }
    }
  }
  return true;
}
