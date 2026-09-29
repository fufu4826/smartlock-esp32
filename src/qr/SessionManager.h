#pragma once

#include <Arduino.h>

enum class SessionType : uint8_t { Setup, Access, Management, Enrollment, Handoff, OwnerBootstrap, Reset, ManagementAuth };

class SessionManager {
 public:
  static constexpr size_t kTokenChars = 64;
  static constexpr size_t kSessionTypeCount = 8;
  bool createSession(SessionType type, uint32_t ttlMs, uint32_t nowMs,
                     char (&out)[kTokenChars + 1]);
  bool validateSession(SessionType type, const char* token, uint32_t nowMs) const;
  bool consumeSession(SessionType type, const char* token, uint32_t nowMs);
  void invalidateSession(SessionType type, const char* token);
  void invalidateAllOfType(SessionType type);
  void expireSessions(uint32_t nowMs);
  bool active(SessionType type, uint32_t nowMs) const;

 private:
  struct Slot {
    char token[kTokenChars + 1] = {};
    uint32_t createdMs = 0;
    uint32_t ttlMs = 0;
    bool valid = false;
  };
  static size_t index(SessionType type);
  static bool matches(const char* a, const char* b);
  mutable Slot slots_[kSessionTypeCount] = {};
};
