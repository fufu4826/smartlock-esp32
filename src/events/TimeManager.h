#pragma once
#include <stdint.h>

// Notification clock only. Authentication/session lifetimes always use monotonic time.
class TimeManager {
 public:
  static void update(bool staConnected, uint32_t nowMs);
  static uint32_t epochSeconds();
  static bool synchronized();
};
