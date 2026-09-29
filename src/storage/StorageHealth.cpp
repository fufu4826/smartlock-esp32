#include "StorageHealth.h"

#include <SD.h>
#include "DeviceStore.h"
#include "UserStore.h"
#include "AuthStore.h"
#include "IdentityStore.h"
#include "AtomicFileStore.h"

bool StorageHealth::begin(bool allowUnconfiguredSetupRepair) {
  available_ = false;
  // Board reference: SD uses VSPI independently of TFT/touch HSPI.
  sdSpi_.begin(18, 19, 23, 5);
  if (!SD.begin(5, sdSpi_, 4000000) || SD.cardType() == CARD_NONE) {
    Serial.println("SD: FAIL (mount/no card)");
    return false;
  }
  const char* directories[] = {
      "/smartlock", "/smartlock/db", "/smartlock/logs",
      "/smartlock/backups", "/smartlock/export"};
  for (const char* path : directories) {
    if (!SD.exists(path) && !SD.mkdir(path)) {
      Serial.println("SD: FAIL (directory)");
      return false;
    }
    File directory = SD.open(path, FILE_READ);
    const bool isDirectory = directory && directory.isDirectory();
    directory.close();
    if (!isDirectory) {
      Serial.println("SD: FAIL (path is not a directory)");
      return false;
    }
  }
  if (allowUnconfiguredSetupRepair && !smartlock::storage::IdentityStore::modePresent() &&
      (!AtomicFileStore::repairUnconfiguredSetupFile("/smartlock/db/users.rec") ||
       !AtomicFileStore::repairUnconfiguredSetupFile("/smartlock/db/devices.rec") ||
       !AtomicFileStore::repairUnconfiguredSetupFile("/smartlock/db/auth.rec"))) {
    Serial.println("SD: FAIL (setup recovery)");
    return false;
  }
  size_t authCount = 0;
  const bool modelValid = smartlock::storage::IdentityStore::modePresent()
      ? smartlock::storage::IdentityStore::healthy()
      : smartlock::storage::UserStore::validate() && smartlock::storage::DeviceStore::validate();
  if (!modelValid ||
      AuthStore::inspect(authCount) == AuthStore::ReadResult::Invalid) {
    Serial.println("SD: FAIL (database integrity)");
    return false;
  }
  available_ = true;
  Serial.printf("SD: OK (%llu MB)\n", SD.cardSize() / 1024 / 1024);
  return true;
}
