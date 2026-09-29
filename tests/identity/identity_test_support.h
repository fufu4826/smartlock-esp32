#pragma once
#include <string>
#include "Arduino.h"
#include "SD.h"
#include "AuthStore.h"
#include "IdentityStore.h"
#include "UserStore.h"
#include "DeviceStore.h"
#include "../archive/event_history/EventLog.h"
#include "EnrollmentManager.h"
#include "AccessController.h"

struct SmartLockConfig {uint32_t unlockDurationMs=5000;bool configured=true;bool ownerExists=true;};
class ConfigStore {
 public:
  const SmartLockConfig& config()const{return config_;}
  SmartLockConfig& mutableConfig(){return config_;}
  bool healthy()const{return healthy_;}
  void setHealthy(bool value){healthy_=value;}
 private: SmartLockConfig config_{};bool healthy_=true;
};
class LockController {
 public:
  enum class UnlockResult {Unlocked,AlreadyUnlocked,InvalidDuration,TimerUnavailable};
  bool isLocked()const{return locked_;}
  UnlockResult unlock(uint32_t duration){++unlockCalls;lastDuration=duration;if(nextResult==UnlockResult::Unlocked)locked_=false;return nextResult;}
  bool locked_=true;unsigned unlockCalls=0;uint32_t lastDuration=0;UnlockResult nextResult=UnlockResult::Unlocked;
};
namespace CanonicalOrigin {const char* host();}
namespace Audit {
void record(smartlock::events::Action,smartlock::events::Result,const char* userId="",const char* deviceId="");
void granted(const char* userId,const char* deviceId,smartlock::events::Source source=smartlock::events::Source::Unknown);
void denied(const char* claimed,const char* verified,smartlock::events::Source source,smartlock::events::Result result=smartlock::events::Result::Denied);
const char* lastUser();const char* lastDevice();
const char* lastClaimed();smartlock::events::Source lastSource();
}
void resetIdentityFakes();
void seedAuth(const char* id,const char* credential);
const std::string& verifierImage();


namespace LineNotifications { extern unsigned emitted; void unlockSuccess(const char*,const char*,const char*,const char*); }
