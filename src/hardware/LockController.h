#pragma once

#include <Arduino.h>
#include <esp_timer.h>

class LockController {
 public:
  static constexpr uint8_t kPin = 22;
  static constexpr uint8_t kLockedLevel = HIGH;
  static constexpr uint8_t kUnlockedLevel = LOW;

  enum class UnlockResult { Unlocked, AlreadyUnlocked, InvalidDuration, TimerUnavailable };

  void begin();
  void lock();
  UnlockResult unlock(uint32_t durationMs);
  void update(uint32_t nowMs);
  bool isLocked() const { return locked_; }
  bool readyForUnlock() const { return timer_ != nullptr; }
  uint32_t remainingUnlockMs(uint32_t nowMs) const;

 private:
  static void timerCallback(void* context);
  volatile bool locked_ = true;
  esp_timer_handle_t timer_ = nullptr;
  uint32_t unlockedAtMs_ = 0;
  uint32_t durationMs_ = 0;
};
