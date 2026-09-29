#include "AccessController.h"
#include "EnrollmentManager.h"
#include "../hardware/LockController.h"
#include "../storage/ConfigStore.h"
#include "../storage/AuthStore.h"
#include "../storage/IdentityStore.h"
#include <string.h>
#include "../notifications/LineNotifications.h"

using namespace smartlock::storage;
AccessController::Result AccessController::request(const char* session,
    const char* deviceId, const char* credential, uint32_t nowMs,
    uint32_t& unlockDurationMs, bool homeLan) {
  unlockDurationMs = 0;
  // Reject wrong type, expired, missing, and consumed tokens before touching
  // persistent data or the lock output.
  if (!sessions_.validateSession(SessionType::Access, session, nowMs)) return Result::Denied;
  if (!config_.healthy() || !config_.config().configured || !config_.config().ownerExists ||
      config_.config().unlockDurationMs < 1000 ||
      config_.config().unlockDurationMs > 60000) return Result::StorageFault;
  if (!IdentityStore::healthy()) return Result::StorageFault;
  IdentityRecord identity = {};
  if (!IdentityStore::find(deviceId, identity) ||
      !AuthStore::verifyDevice(deviceId, credential)) return Result::Denied;
  if (identity.status != RecordStatus::Active) return Result::Denied;
  // Recovery AP remains Owner-only. LAN allows explicitly active door roles.
  if (identity.role != UserRole::Owner && !(homeLan &&
      (identity.role == UserRole::Admin || identity.role == UserRole::User ||
       identity.role == UserRole::Guest))) return Result::Denied;
  if (!lock_.isLocked()) {
    sessions_.consumeSession(SessionType::Access, session, millis());
    return Result::AlreadyUnlocked;
  }
  if (!sessions_.consumeSession(SessionType::Access, session, millis())) return Result::Denied;
  const uint32_t duration = config_.config().unlockDurationMs;
  if (lock_.unlock(duration) != LockController::UnlockResult::Unlocked) return Result::Denied;
  unlockDurationMs = duration;
  const char* role = identity.role == UserRole::Owner ? "OWNER" :
      identity.role == UserRole::Admin ? "ADMIN" : identity.role == UserRole::Guest ? "GUEST" : "USER";
  LineNotifications::unlockSuccess(identity.id, identity.name, role, homeLan ? "LAN" : "Recovery AP");
  return Result::Unlocked;
}
