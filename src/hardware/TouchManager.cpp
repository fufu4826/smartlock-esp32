#include "TouchManager.h"

void TouchManager::begin(TFT_eSPI& tft) {
  tft_ = &tft;

  rawTouch_ = false;
  candidateTouch_ = false;
  stableTouch_ = false;
  candidateSinceMs_ = 0;

  tapCount_ = 0;
  firstTapMs_ = 0;
  lastTapMs_ = 0;
  pressedSinceMs_ = 0;
  holdFired_ = false;
}

TouchEvent TouchManager::update(uint32_t nowMs) {
  if (tft_ == nullptr) {
    return TouchEvent::None;
  }

  uint16_t x = 0;
  uint16_t y = 0;
  rawTouch_ = tft_->getTouch(&x, &y, kTouchThreshold);

  if (rawTouch_ != candidateTouch_) {
    candidateTouch_ = rawTouch_;
    candidateSinceMs_ = nowMs;
  }

  if (candidateTouch_ == stableTouch_ ||
      static_cast<uint32_t>(nowMs - candidateSinceMs_) < kDebounceMs) {
    if (stableTouch_ && candidateTouch_ && !holdFired_ &&
        static_cast<uint32_t>(nowMs - pressedSinceMs_) >= kHoldMs) {
      holdFired_ = true;
      tapCount_ = 0;
      firstTapMs_ = 0;
      lastTapMs_ = 0;
      return TouchEvent::Hold;
    }
    return TouchEvent::None;
  }

  stableTouch_ = candidateTouch_;
  if (!stableTouch_) {
    return holdFired_ ? TouchEvent::None : TouchEvent::TapReleased;
  }
  pressedSinceMs_ = nowMs;
  holdFired_ = false;
  pressX_ = x;
  pressY_ = y;
  return TouchEvent::SingleTap;
}
