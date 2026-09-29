#include "FactoryResetController.h"

#include <Preferences.h>
#include <SD.h>
#include <WiFi.h>
#include <string.h>

#include "../hardware/LockController.h"
#include "../qr/SessionManager.h"
#include "../storage/AuthStore.h"
#include "EnrollmentManager.h"
#include "../notifications/LineNotifications.h"

bool FactoryResetController::eraseSmartLockTree(const String& path,
                                                unsigned depth) {
  if (depth > 8 || !path.startsWith("/smartlock")) return false;
  File directory = SD.open(path);
  if (!directory || !directory.isDirectory()) return false;
  File child;
  while ((child = directory.openNextFile())) {
    String name = child.name();
    const int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    if (!name.length() || name == "." || name == "..") {
      child.close(); directory.close(); return false;
    }
    const String childPath = path + "/" + name;
    const bool isDirectory = child.isDirectory();
    child.close();
    if (isDirectory ? !eraseSmartLockTree(childPath, depth + 1)
                    : !SD.remove(childPath)) {
      directory.close(); return false;
    }
  }
  directory.close();
  return SD.rmdir(path);
}

bool FactoryResetController::clearNamespace(const char* name) {
  Preferences nvs;
  if (!nvs.begin(name, false)) return false;
  const bool cleared = nvs.clear();
  nvs.end();
  return cleared;
}

FactoryResetController::Result FactoryResetController::request(
    const char* session, const char* deviceId, const char* credential,
    uint32_t nowMs) {
  if (restartScheduled_ || !lock_.isLocked() ||
      !sessions_.validateSession(SessionType::Reset, session, nowMs) ||
      !deviceId || !credential ||
      !AuthStore::verifyDevice(deviceId, credential)) return Result::Denied;

  size_t userCount = 0, deviceCount = 0, active = 0, revoked = 0;
  if (!enrollment_.databaseIntegrity(userCount, deviceCount, active, revoked))
    return Result::StorageFailure;
  if (!enrollment_.isActiveOwnerDevice(deviceId) ||
      !sessions_.consumeSession(SessionType::Reset, session, millis()))
    return Result::Denied;

  return performReset();
}

FactoryResetController::Result FactoryResetController::requestPhysical(bool physicalAuthorized) {
  if (!physicalAuthorized || restartScheduled_) return Result::Denied;
  return performReset();
}

FactoryResetController::Result FactoryResetController::performReset() {
  lock_.lock();
  if(LineNotifications::suspendForReset()!=LineNotifications::Result::Ok)return Result::StorageFailure;
  if(digitalRead(LockController::kPin)!=LockController::kLockedLevel)return Result::StorageFailure;
  sessions_.invalidateAllOfType(SessionType::Access);
  sessions_.invalidateAllOfType(SessionType::Management);
  sessions_.invalidateAllOfType(SessionType::ManagementAuth);
  sessions_.invalidateAllOfType(SessionType::Enrollment);
  sessions_.invalidateAllOfType(SessionType::OwnerBootstrap);
  // Driver candidates now use RAM storage. Explicitly erase legacy SDK NVS
  // credentials too; disconnect(..., true) alone would only clear RAM.
  // This is reachable only after the existing authorized reset confirmation.
  if (!WiFi.eraseAP()) return Result::StorageFailure;
  // Prototype policy: sl-line (LINE token, enabled state, Basic ID) and touch
  // calibration (lock-touch-v2) are deliberately preserved across reset.
  if ((SD.exists("/smartlock") && !eraseSmartLockTree("/smartlock", 0)) ||
      SD.exists("/smartlock") ||
      !clearNamespace("sl-config") || !clearNamespace("sl-net") ||
      !clearNamespace("sl-install") ||
      !clearNamespace("sl-pin") || !clearNamespace("sl-generation") || !clearNamespace("sl-ota") || !clearNamespace("sl-sta") || !clearNamespace("sl-cloud") || !clearNamespace("sl-audit") || !clearNamespace("p2-test"))
    return Result::StorageFailure;

  restartScheduled_ = true;
  restartAtMs_ = millis() + 1500;
  return Result::ResetScheduled;
}

bool FactoryResetController::restartDue(uint32_t nowMs) const {
  return restartScheduled_ &&
         static_cast<int32_t>(nowMs - restartAtMs_) >= 0;
}
