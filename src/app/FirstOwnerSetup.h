#pragma once

#include <Arduino.h>
#include "../qr/SessionManager.h"
#include "../storage/ConfigStore.h"
#include "../storage/NetworkSecrets.h"
#include "../storage/StorageHealth.h"

struct FirstOwnerInput {
  char session[SessionManager::kTokenChars + 1];
  char ownerName[41];
  char credential[65];
  char apPassword[33];
  uint32_t unlockSeconds;
  char adminPin[5];
  char pinConfirm[5];
};

class FirstOwnerSetup {
 public:
  enum class Result {
    Success, InvalidInput, SessionRejected, AlreadyConfigured,
    StorageUnavailable, DatabaseConflict, PersistenceFailure
  };
  FirstOwnerSetup(ConfigStore& config, StorageHealth& storage,
                  SessionManager& sessions, NetworkSecrets& secrets)
      : config_(config), storage_(storage), sessions_(sessions), secrets_(secrets) {}
  Result complete(const FirstOwnerInput& input, uint32_t nowMs);
  bool configuredDataValid() const;
  bool restartDue(uint32_t nowMs) const {
    return finished_ && static_cast<uint32_t>(nowMs - finishedAtMs_) >= 8000;
  }

 private:
  static bool lowerHex(const char* value, size_t length);
  bool existingSetupFilesCompatible() const;
  ConfigStore& config_;
  StorageHealth& storage_;
  SessionManager& sessions_;
  NetworkSecrets& secrets_;
  bool finished_ = false;
  uint32_t finishedAtMs_ = 0;
};
