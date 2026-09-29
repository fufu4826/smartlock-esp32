#pragma once

#include <Arduino.h>
#include <IPAddress.h>

// Addressing only: this class never reads credentials or authorization data.
class CanonicalOrigin {
 public:
  static const char* label();
  static const char* host();
  static bool begin();
  // Called from the normal loop. Interface address changes settle before an
  // explicit mDNS restart; failed starts/restarts are retried on a timer.
  static void update(uint32_t nowMs, IPAddress staIp, IPAddress apIp);
  static bool ready();
};
