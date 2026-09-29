#include "DisplayManager.h"

#include "ThaiResetBitmaps.h"
// Physical reset confirmations use dedicated Thai bitmap labels.

namespace {
template <size_t N>
void drawCenteredBitmap(TFT_eSPI& tft, const uint8_t (&bitmap)[N],
                        int16_t width, int16_t height, int16_t centerX,
                        int16_t y, uint16_t color) {
  tft.drawBitmap(centerX - width / 2, y, bitmap, width, height, color);
}

void drawResetButton(TFT_eSPI& tft, int16_t x, int16_t width,
                     const uint8_t* label, int16_t labelWidth,
                     int16_t labelHeight) {
  constexpr int16_t y = 300;
  constexpr int16_t height = 70;
  tft.fillRoundRect(x, y, width, height, 12, TFT_WHITE);
  tft.drawRoundRect(x, y, width, height, 12, TFT_BLACK);
  tft.drawBitmap(x + (width - labelWidth) / 2,
                 y + (height - labelHeight) / 2,
                 label, labelWidth, labelHeight, TFT_BLACK);
}

void drawResetRequest(TFT_eSPI& tft) {
  const int16_t centerX = tft.width() / 2;
  tft.fillScreen(TFT_RED);
  drawCenteredBitmap(tft, kThaiResetQuestionLine1, kThaiResetQuestionLine1Width,
                     kThaiResetQuestionLine1Height, centerX, 158, TFT_WHITE);
  drawCenteredBitmap(tft, kThaiResetQuestionLine2, kThaiResetQuestionLine2Width,
                     kThaiResetQuestionLine2Height, centerX, 204, TFT_WHITE);
  drawResetButton(tft, 20, 130, kThaiYes, kThaiYesWidth, kThaiYesHeight);
  drawResetButton(tft, 170, 130, kThaiNo, kThaiNoWidth, kThaiNoHeight);
}

void drawResetConfirm(TFT_eSPI& tft) {
  const int16_t centerX = tft.width() / 2;
  tft.fillScreen(TFT_RED);
  drawCenteredBitmap(tft, kThaiResetConfirmLine1, kThaiResetConfirmLine1Width,
                     kThaiResetConfirmLine1Height, centerX, 158, TFT_WHITE);
  drawCenteredBitmap(tft, kThaiResetConfirmLine2, kThaiResetConfirmLine2Width,
                     kThaiResetConfirmLine2Height, centerX, 204, TFT_WHITE);
  drawResetButton(tft, 60, 200, kThaiOkay, kThaiOkayWidth, kThaiOkayHeight);
}
}  // namespace

void DisplayManager::begin(TFT_eSPI& tft) {
  tft_ = &tft;
  pinMode(kBacklightPin, OUTPUT);
  digitalWrite(kBacklightPin, LOW);
}

void DisplayManager::backlightOn() {
  digitalWrite(kBacklightPin, HIGH);
}

void DisplayManager::backlightOff() {
  digitalWrite(kBacklightPin, LOW);
}

void DisplayManager::render(AppState state) {
  if (tft_ == nullptr) return;
  if (state == AppState::IdleScreenOff) {
    backlightOff();
    Serial.println("SCREEN: OFF");
    return;
  }

  if (state == AppState::ResetRequest) {
    drawResetRequest(*tft_);
    backlightOn();
    Serial.println("SCREEN: RESET CONFIRMATION QUESTION");
    return;
  }
  if (state == AppState::ResetConfirm) {
    drawResetConfirm(*tft_);
    backlightOn();
    Serial.println("SCREEN: RESET FINAL CONFIRMATION");
    return;
  }
  if (state == AppState::Resetting) {
    tft_->fillScreen(TFT_RED);
    drawCenteredBitmap(*tft_, kThaiResetting, kThaiResettingWidth,
                       kThaiResettingHeight, tft_->width() / 2,
                       tft_->height() / 2 - kThaiResettingHeight / 2,
                       TFT_WHITE);
    backlightOn();
    Serial.println("SCREEN: RESETTING");
    return;
  }

  const char* top = "ERROR";
  const char* bottom = "STATUS";
  switch (state) {
    case AppState::SetupRequest:
      top = "SETUP";
      bottom = "REQUEST";
      break;
    case AppState::AccessRequest:
      top = "ACCESS";
      bottom = "REQUEST";
      break;
    case AppState::ManagementRequest:
      top = "MANAGEMENT";
      bottom = "REQUEST";
      break;
    default:
      break;
  }

  // Font 4 is explicitly enabled in platformio.ini. The previous phase-one
  // renderer selected font 2, which was not built into this firmware.
  tft_->fillScreen(TFT_BLACK);
  tft_->setTextDatum(MC_DATUM);
  tft_->setTextColor(TFT_WHITE, TFT_BLACK);
  const int centerX = tft_->width() / 2;
  const int centerY = tft_->height() / 2;
  tft_->drawString(top, centerX, centerY - 24, 4);
  tft_->drawString(bottom, centerX, centerY + 24, 4);
  backlightOn();
  Serial.print("SCREEN: ");
  Serial.print(top);
  Serial.print(' ');
  Serial.println(bottom);
}
