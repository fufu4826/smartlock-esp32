#pragma once

#include <stddef.h>
#include <stdint.h>

#include "phone2_test_base.h"
#include "EnrollmentManager.h"
#include "AccessController.h"

namespace smartlock { namespace storage {
class UserStore {
 public:
  static const size_t kMaxRecords = 64;
  static bool load(UserRecord*, size_t, size_t&);
  static bool replace(const UserRecord*, size_t);
};
class DeviceStore {
 public:
  static const size_t kMaxRecords = 64;
  static bool load(DeviceRecord*, size_t, size_t&);
  static bool replace(const DeviceRecord*, size_t);
};
} }

class AuthStore {
 public:
  enum class ReadResult { Valid, Missing, Invalid };
  static bool hasDevice(const char*);
  static bool verifyDevice(const char*, const char*);
  static bool addDevice(const char*, const char*);
  static ReadResult inspect(size_t&);
};

namespace CanonicalOrigin { const char* host(); }
namespace Audit {
void record(smartlock::events::Action, smartlock::events::Result,
            const char* userId = "", const char* deviceId = "");
void granted(const char* userId, const char* deviceId);
const char* lastUser();
const char* lastDevice();
}

struct SmartLockConfig { uint32_t unlockDurationMs = 5000; bool configured = true; bool ownerExists = true; };
class ConfigStore {
 public:
  const SmartLockConfig& config() const { return config_; }
  SmartLockConfig& mutableConfig() { return config_; }
  bool healthy() const { return healthy_; }
  void setHealthy(bool value) { healthy_ = value; }
 private:
  SmartLockConfig config_;
  bool healthy_ = true;
};

class LockController {
 public:
  enum class UnlockResult { Unlocked, AlreadyUnlocked, InvalidDuration, TimerUnavailable };
  bool isLocked() const { return locked; }
  UnlockResult unlock(uint32_t duration) {
    ++unlockCalls; lastDuration = duration;
    if (nextUnlockResult == UnlockResult::Unlocked) locked = false;
    return nextUnlockResult;
  }
  bool locked = true;
  unsigned unlockCalls = 0;
  uint32_t lastDuration = 0;
  UnlockResult nextUnlockResult = UnlockResult::Unlocked;
};

void resetPhone2Fakes();
void setPhone2StoreFailures(bool userLoad, bool deviceLoad, bool authInspect);
bool fakeUser(const char* id, smartlock::storage::UserRole* role = nullptr,
              smartlock::storage::RecordStatus* status = nullptr,
              const char** name = nullptr);
bool fakeDevice(const char* id, const char** userId = nullptr, const char** name = nullptr,
                smartlock::storage::RecordStatus* status = nullptr);
size_t fakeDeviceCount();
size_t fakeUserCount();
