#pragma once

#include <stdint.h>

namespace LineNotifications {

enum class Result : uint8_t {
  Ok, InvalidInput, NotConfigured, RateLimited, StorageError
};

enum class State : uint8_t {
  Disabled, Ready, Offline, QuotaFull, AuthRequired, ServiceError, TimeUnavailable
};

struct Status {
  bool configured;
  bool enabled;
  bool staConnected;
  bool quotaKnown;
  uint8_t testState; // 0 none, 1 queued, 2 API accepted, 3 failed
  uint16_t configVersion;
  uint32_t queued;
  uint32_t sentThisRun;
  uint32_t failedThisRun;
  uint16_t quotaLimit;
  uint16_t quotaUsed;
  State state;
  char deviceLabel[65];
  char publicBasicId[65];
};

// begin/update are safe to call from the main loop. They perform no network I/O.
void begin(bool staConnected);
void update(bool staConnected);
// Broadcast configuration: no recipient User ID (all OA friends receive notifications).
Result save(const char* label, const char* token, const char* publicBasicId);
Result rename(const char* label);
Result disconnect();
// Factory Reset only: stops runtime delivery until reboot without touching persisted config.
Result suspendForReset();
void status(Status& out);
struct Telemetry {
  uint32_t unlockGenerated, pinGenerated, emergencyGenerated, enqueued;
  uint32_t pushStarted, tlsConnected, pushWritten, responses, heapBlocked;
  uint32_t admissionFree, admissionLargest, admissionMinimum;
  uint32_t tlsMinimum, tlsAfterLargest;
  uint16_t lastPushHttp;
};
void telemetry(Telemetry& out);
Result requestTest();

// Emitters only copy bounded event metadata into the RAM queue. They never block on LINE.
void unlockSuccess(const char* id, const char* name, const char* role, const char* source);
void pinFailed(uint8_t count, bool locked, bool lockout);
void emergencyUnlock();

}  // namespace LineNotifications
