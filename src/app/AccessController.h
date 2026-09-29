#pragma once
#include <Arduino.h>
#include "../qr/SessionManager.h"

class LockController;
class ConfigStore;
class EnrollmentManager;

class AccessController {
 public:
  enum class Result { Unlocked, Denied, AlreadyUnlocked, StorageFault };
  AccessController(SessionManager& sessions, LockController& lock,
                   ConfigStore& config, EnrollmentManager& enrollment)
      : sessions_(sessions), lock_(lock), config_(config), enrollment_(enrollment) {}
  Result request(const char* session, const char* deviceId,
                 const char* credential, uint32_t nowMs,
                 uint32_t& unlockDurationMs, bool homeLan = false);
  void suspend(){sessions_.invalidateAllOfType(SessionType::Access);}
 private:
  SessionManager& sessions_;
  LockController& lock_;
  ConfigStore& config_;
  EnrollmentManager& enrollment_;
};
