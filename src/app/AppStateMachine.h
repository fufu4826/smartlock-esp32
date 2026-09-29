#pragma once

#include <Arduino.h>
#include "AppState.h"

class AppStateMachine {
 public:
  void begin(uint32_t nowMs, bool configured);
  void onSingleTap(uint32_t nowMs);
  void onTapReleased(uint32_t nowMs);
  void onTripleTap(uint32_t nowMs);
  void onHold(uint32_t nowMs);
  void resetChoice(bool yes, uint32_t nowMs);
  void resetStarted(uint32_t nowMs);
  void update(uint32_t nowMs);
  void refreshRequest(uint32_t nowMs) {
    if(state_==AppState::AccessRequest||state_==AppState::ManagementRequest)
      transition(state_,nowMs);
  }
  AppState state() const { return state_; }
  bool configured() const { return configured_; }
  bool changed() const { return changed_; }
  void acknowledgeChange() { changed_ = false; }

 private:
  void transition(AppState next, uint32_t nowMs);
  AppState state_ = AppState::Boot;
  uint32_t shownAtMs_ = 0;
  bool changed_ = false;
  bool configured_ = false;
};
