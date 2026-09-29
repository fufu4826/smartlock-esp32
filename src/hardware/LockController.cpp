#include "LockController.h"

void LockController::begin() {
  // Preload before output enable to minimize a startup glitch.
  digitalWrite(kPin, kLockedLevel);
  pinMode(kPin, OUTPUT);
  digitalWrite(kPin, kLockedLevel);
  locked_ = true;
  durationMs_ = 0;
  const esp_timer_create_args_t args = {
      .callback = &LockController::timerCallback,
      .arg = this,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "lock-relock"};
  if (esp_timer_create(&args, &timer_) != ESP_OK) timer_ = nullptr;
}

void LockController::lock() {
  if (timer_) esp_timer_stop(timer_);
  digitalWrite(kPin, kLockedLevel);
  locked_ = true;
  durationMs_ = 0;
}

LockController::UnlockResult LockController::unlock(uint32_t durationMs) {
  if (!locked_) return UnlockResult::AlreadyUnlocked;
  // Durations stay below half the millis() wrap period for safe subtraction.
  if (durationMs == 0 || durationMs > 60000) return UnlockResult::InvalidDuration;
  if (!timer_ || esp_timer_start_once(timer_, static_cast<uint64_t>(durationMs) * 1000) != ESP_OK)
    return UnlockResult::TimerUnavailable;
  unlockedAtMs_ = millis();
  durationMs_ = durationMs;
  digitalWrite(kPin, kUnlockedLevel);
  locked_ = false;
  return UnlockResult::Unlocked;
}

void LockController::timerCallback(void* context) {
  auto* controller = static_cast<LockController*>(context);
  digitalWrite(kPin, kLockedLevel);
  controller->locked_ = true;
  controller->durationMs_ = 0;
  Serial.println("LOCK STATE: LOCKED (hardware timer)");
}

void LockController::update(uint32_t nowMs) {
  if (!locked_ && static_cast<uint32_t>(nowMs - unlockedAtMs_) >= durationMs_) {
    lock();
    Serial.println("LOCK STATE: LOCKED (timer)");
  }
}

uint32_t LockController::remainingUnlockMs(uint32_t nowMs) const {
  if (locked_) return 0;
  const uint32_t elapsed = static_cast<uint32_t>(nowMs - unlockedAtMs_);
  return elapsed >= durationMs_ ? 0 : durationMs_ - elapsed;
}
