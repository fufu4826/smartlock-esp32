#include "Phase2SelfTest.h"

#ifdef PHASE2_SELFTEST
#include <Arduino.h>
#include <Preferences.h>
#include <SD.h>
#include <string.h>

#include "AtomicFileStore.h"
#include "DeviceStore.h"
#include "UserStore.h"

using smartlock::storage::DeviceRecord;
using smartlock::storage::DeviceStore;
using smartlock::storage::RecordStatus;
using smartlock::storage::UserRecord;
using smartlock::storage::UserRole;
using smartlock::storage::UserStore;

namespace {
constexpr char kUserPath[] = "/smartlock/db/users.rec";
constexpr char kDevicePath[] = "/smartlock/db/devices.rec";
constexpr char kProbePath[] = "/smartlock/db/p2-integrity.rec";

void fail(const char* step) {
  Serial.print("PHASE2 TEST: FAIL ");
  Serial.println(step);
}

void advanceAndRestart(Preferences& prefs, uint8_t next) {
  if (prefs.putUChar("stage", next) != 1) {
    fail("NVS stage write");
    return;
  }
  Serial.printf("PHASE2 TEST: stage %u PASS, rebooting\n", next - 1);
  Serial.flush();
  delay(200);
  ESP.restart();
}

bool verifyRecords(const char* userName, const char* deviceName) {
  UserRecord users[2] = {};
  DeviceRecord devices[2] = {};
  size_t userCount = 0, deviceCount = 0;
  return UserStore::load(users, 2, userCount) &&
         DeviceStore::load(devices, 2, deviceCount) &&
         userCount == 2 && deviceCount == 2 &&
         strcmp(users[0].id, "UTEST001") == 0 &&
         strcmp(users[0].name, userName) == 0 &&
         strcmp(users[1].id, "UTEST002") == 0 &&
         strcmp(devices[0].id, "DTEST001") == 0 &&
         strcmp(devices[0].name, deviceName) == 0 &&
         strcmp(devices[1].id, "DTEST002") == 0;
}
}

void runPhase2SelfTest() {
  Preferences prefs;
  if (!prefs.begin("p2-test", false)) {
    fail("NVS open");
    return;
  }
  const uint8_t stage = prefs.getUChar("stage", 0);
  Serial.printf("PHASE2 TEST: stage %u\n", stage);
  if (stage == 0) {
    // Only run against a fresh Phase 2 database. Never overwrite user data.
    if (SD.exists(kUserPath) || SD.exists(kDevicePath)) {
      fail("database already present; preserving it");
      return;
    }
    UserRecord users[2] = {
        {"UTEST001", "Test Owner", UserRole::Owner, RecordStatus::Active},
        {"UTEST002", "Test User", UserRole::User, RecordStatus::Active}};
    DeviceRecord devices[2] = {
        {"DTEST001", "UTEST001", "Test Phone 1", RecordStatus::Active},
        {"DTEST002", "UTEST002", "Test Phone 2", RecordStatus::Active}};
    if (!UserStore::replace(users, 2) || !DeviceStore::replace(devices, 2) ||
        !verifyRecords("Test Owner", "Test Phone 1")) {
      fail("create/read multiple records");
      return;
    }
    advanceAndRestart(prefs, 1);
  } else if (stage == 1) {
    if (!verifyRecords("Test Owner", "Test Phone 1")) {
      fail("persistence after restart");
      return;
    }
    UserRecord users[2] = {
        {"UTEST001", "Owner Edited", UserRole::Owner, RecordStatus::Active},
        {"UTEST002", "Test User", UserRole::User, RecordStatus::Active}};
    DeviceRecord devices[2] = {
        {"DTEST001", "UTEST001", "Phone Edited", RecordStatus::Active},
        {"DTEST002", "UTEST002", "Test Phone 2", RecordStatus::Active}};
    if (!UserStore::replace(users, 2) || !DeviceStore::replace(devices, 2) ||
        !verifyRecords("Owner Edited", "Phone Edited")) {
      fail("modify/read records");
      return;
    }
    advanceAndRestart(prefs, 2);
  } else if (stage == 2) {
    if (!verifyRecords("Owner Edited", "Phone Edited")) {
      fail("modified records after restart");
      return;
    }
    if (!UserStore::replace(nullptr, 0) || !DeviceStore::replace(nullptr, 0)) {
      fail("remove test records");
      return;
    }
    advanceAndRestart(prefs, 3);
  } else if (stage == 3) {
    size_t users = 99, devices = 99;
    if (!UserStore::load(nullptr, 0, users) ||
        !DeviceStore::load(nullptr, 0, devices) || users != 0 || devices != 0) {
      fail("removed records after restart");
      return;
    }
    // These backup files were created only after stage 0 confirmed the paths
    // were absent. Remove the exact diagnostic backups after empty current
    // files have survived a reboot, so no test identities remain on the card.
    const char* backups[] = {
        "/smartlock/db/users.rec.bak", "/smartlock/db/devices.rec.bak"};
    for (const char* path : backups) {
      if (SD.exists(path) && !SD.remove(path)) {
        fail("test backup cleanup");
        return;
      }
    }
    if (SD.exists(kProbePath)) {
      fail("probe path already exists; preserving it");
      return;
    }
    const uint8_t probe[] = {'P', '2', 'O', 'K'};
    if (!AtomicFileStore::write(kProbePath, probe, sizeof(probe))) {
      fail("integrity probe write");
      return;
    }
    File file = SD.open(kProbePath, FILE_APPEND);
    if (!file || file.write(static_cast<uint8_t>('X')) != 1) {
      fail("integrity probe corruption");
      return;
    }
    file.flush();
    file.close();
    uint8_t output[16] = {};
    size_t length = 0;
    const bool rejected = AtomicFileStore::read(kProbePath, output, sizeof(output), length) ==
                          AtomicFileStore::ReadResult::Corrupt;
    if (!rejected || !SD.remove(kProbePath)) {
      fail("corrupt file rejection/cleanup");
      return;
    }
    if (prefs.putUChar("stage", 4) != 1) {
      fail("completion marker");
      return;
    }
    Serial.println("PHASE2 TEST: PASS create/reload/modify/reload/remove/corrupt-reject");
  } else if (stage == 4) {
    size_t users = 99, devices = 99;
    if (UserStore::load(nullptr, 0, users) &&
        DeviceStore::load(nullptr, 0, devices) && users == 0 && devices == 0) {
      const char* backups[] = {
          "/smartlock/db/users.rec.bak", "/smartlock/db/devices.rec.bak"};
      for (const char* path : backups) {
        if (SD.exists(path) && !SD.remove(path)) {
          fail("test backup cleanup");
          return;
        }
      }
      Serial.println("PHASE2 TEST: COMPLETE (empty DB and backup cleanup verified)");
    } else {
      fail("completion readback");
    }
  } else {
    fail("unknown stage");
  }
  prefs.end();
}
#else
void runPhase2SelfTest() {}
#endif
