#pragma once

#include <Arduino.h>

class SessionManager;
class LockController;
class EnrollmentManager;

class FactoryResetController {
 public:
  enum class Result { ResetScheduled, Denied, StorageFailure };
  FactoryResetController(SessionManager& sessions, LockController& lock,
                         EnrollmentManager& enrollment)
      : sessions_(sessions), lock_(lock), enrollment_(enrollment) {}
  Result request(const char* session, const char* deviceId,
                 const char* credential, uint32_t nowMs);
  bool restartDue(uint32_t nowMs) const;
  Result requestPhysical(bool physicalAuthorized = false);
  bool restartPending() const { return restartScheduled_; }

 private:
  static bool eraseSmartLockTree(const String& path, unsigned depth);
  static bool clearNamespace(const char* name);
  Result performReset();
  SessionManager& sessions_;
  LockController& lock_;
  EnrollmentManager& enrollment_;
  bool restartScheduled_ = false;
  uint32_t restartAtMs_ = 0;
};
