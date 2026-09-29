#include "FirstOwnerSetup.h"
#include "../security/AdminPin.h"

#include "../storage/AuthStore.h"
#include "../storage/IdentityStore.h"
#include <string.h>
#include <memory>
#include <new>

using namespace smartlock::storage;

bool FirstOwnerSetup::lowerHex(const char* value, size_t length) {
  if (!value || strnlen(value, length + 1) != length) return false;
  for (size_t i = 0; i < length; ++i) {
    const char c = value[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

bool FirstOwnerSetup::existingSetupFilesCompatible() const {
  // Never overwrite existing credentials to recover a partial setup.
  size_t count=0;
  return !IdentityStore::modePresent() && AuthStore::inspect(count)==AuthStore::ReadResult::Missing;

}

bool FirstOwnerSetup::configuredDataValid() const {
  if (!config_.healthy() || !storage_.available() ||
      !config_.config().configured || !config_.config().ownerExists) return false;
  char password[33] = {};
  if (secrets_.load(password) != NetworkSecrets::Result::Valid) return false;
  memset(password, 0, sizeof(password));
  return IdentityStore::healthy();

}

FirstOwnerSetup::Result FirstOwnerSetup::complete(const FirstOwnerInput& input,
                                                   uint32_t nowMs) {
  if (!config_.healthy() || config_.config().configured || config_.config().ownerExists ||
      finished_) return Result::AlreadyConfigured;
  if (!storage_.available()) return Result::StorageUnavailable;
  if (!sessions_.validateSession(SessionType::Setup, input.session, nowMs))
    return Result::SessionRejected;
  if (!lowerHex(input.credential, 64) || !lowerHex(input.apPassword, 32) ||
      input.unlockSeconds < 1 || input.unlockSeconds > 60 ||
      !AdminPin::valid(input.adminPin) || !AdminPin::valid(input.pinConfirm) ||
      strcmp(input.adminPin, input.pinConfirm))
    return Result::InvalidInput;
  char ownerName[41] = {};
  if (!IdentityStore::normalizeName(input.ownerName, ownerName)) return Result::InvalidInput;
  if (!existingSetupFilesCompatible()) return Result::DatabaseConflict;
  if (!sessions_.consumeSession(SessionType::Setup, input.session, nowMs))
    return Result::SessionRejected;

  // The configured NVS flags are the commit marker and are written LAST.
  // A power cut in any earlier step leaves the device unconfigured and locked.
  if (!secrets_.saveSetupPassword(input.apPassword) ||
      !AuthStore::saveFirst("D000001", input.credential) ||
      !IdentityStore::createFirst(ownerName))
    return Result::PersistenceFailure;

  if (!IdentityStore::healthy() ||
      !AuthStore::verifyFirst("D000001", input.credential))
    return Result::PersistenceFailure;
  char savedPassword[33] = {};
  if (secrets_.load(savedPassword) != NetworkSecrets::Result::Valid ||
      strcmp(savedPassword, input.apPassword) != 0) return Result::PersistenceFailure;
  memset(savedPassword, 0, sizeof(savedPassword));

  SmartLockConfig next = config_.config();
  next.unlockDurationMs = input.unlockSeconds * 1000;
  next.configured = 1;
  next.ownerExists = 1;
  next.networkMode = 0; // configured AP mode
  if (!AdminPin::initialize(input.adminPin) || !config_.save(next)) return Result::PersistenceFailure;
  finishedAtMs_ = millis();
  finished_ = true;
  return Result::Success;
}
