#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

enum class TouchEvent {
  None,
  SingleTap,
  TripleTap,
  Hold,
  TapReleased,
};

class TouchManager {
 public:
  void begin(TFT_eSPI& tft);
  TouchEvent update(uint32_t nowMs);
  uint16_t x() const { return pressX_; }
  uint16_t y() const { return pressY_; }

 private:
  static constexpr uint32_t kDebounceMs = 65;
  static constexpr uint32_t kTripleTapWindowMs = 1200;
  static constexpr uint32_t kMaximumTapGapMs = 500;
  static constexpr uint32_t kHoldMs = 1500;
  static constexpr uint16_t kTouchThreshold = 600;

  TFT_eSPI* tft_ = nullptr;

  bool rawTouch_ = false;
  bool candidateTouch_ = false;
  bool stableTouch_ = false;
  uint32_t candidateSinceMs_ = 0;

  uint8_t tapCount_ = 0;
  uint32_t firstTapMs_ = 0;
  uint32_t lastTapMs_ = 0;
  uint32_t pressedSinceMs_ = 0;
  bool holdFired_ = false;
  uint16_t pressX_ = 0, pressY_ = 0;
};
