#pragma once
#include <stdint.h>
#include "ConfigStore.h"
class EnrollmentManager {
 public:
  bool allow = true;
  uint32_t expiresAt = UINT32_MAX;
  bool authorizedOwner(const char*, uint32_t now) {
    return allow && now <= expiresAt;
  }
};
