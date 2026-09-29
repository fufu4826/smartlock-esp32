#pragma once

#include <Arduino.h>
#include "../storage/RecordCodec.h"

// Read-only proof that an exact pending browser credential belongs to an
// already committed, active registration. This service never consumes grants
// or changes identities/verifiers.
class RegistrationRecovery {
 public:
  enum class Kind { Setup, Enrollment };
  static bool reconcile(Kind kind, const char* credential, char (&deviceId)[9],
                        smartlock::storage::UserRole& role);
};
