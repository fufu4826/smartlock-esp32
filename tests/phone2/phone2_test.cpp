#include "phone2_test_support.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

using namespace smartlock::storage;
namespace {
int checks = 0;
void check(bool condition, const char* description) {
  ++checks;
  if (!condition) { std::cerr << "FAIL: " << description << '\n'; std::exit(1); }
}

const char kOwnerCredential[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kAdminCredential[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
const char kPhoneOneCredential[] = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
const char kPhoneTwoCredential[] = "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd";

void addUserRecord(const char* id, const char* name, UserRole role) {
  UserRecord records[UserStore::kMaxRecords] = {};
  size_t count = 0;
  check(UserStore::load(records, UserStore::kMaxRecords, count), "fake user store loads");
  UserRecord& record = records[count++];
  std::snprintf(record.id, sizeof(record.id), "%s", id);
  std::snprintf(record.name, sizeof(record.name), "%s", name);
  record.role = role; record.status = RecordStatus::Active;
  check(UserStore::replace(records, count), "fake user record stored");
}

void addDeviceRecord(const char* id, const char* userId, const char* name,
                     const char* credential) {
  DeviceRecord records[DeviceStore::kMaxRecords] = {};
  size_t count = 0;
  check(DeviceStore::load(records, DeviceStore::kMaxRecords, count), "fake device store loads");
  DeviceRecord& record = records[count++];
  std::snprintf(record.id, sizeof(record.id), "%s", id);
  std::snprintf(record.userId, sizeof(record.userId), "%s", userId);
  std::snprintf(record.name, sizeof(record.name), "%s", name);
  record.status = RecordStatus::Active;
  check(DeviceStore::replace(records, count), "fake device record stored");
  check(AuthStore::addDevice(id, credential), "fake auth entry stored");
}

void seedOwner() {
  addUserRecord("U000001", "Owner", UserRole::Owner);
  addDeviceRecord("D000001", "U000001", "Owner phone", kOwnerCredential);
}

bool managementLogin(SessionManager& sessions, EnrollmentManager& manager,
                     const char* deviceId, const char* credential, uint32_t now,
                     char (&bearer)[65]) {
  setFakeMillis(now);
  char qr[65] = {};
  if (!sessions.createSession(SessionType::Management, 60000, now, qr)) return false;
  return manager.login(qr, deviceId, credential, now, bearer);
}

std::string sessionFromUrl(const char* url) {
  const char* marker = std::strstr(url, "session=");
  return marker ? marker + 8 : "";
}

void testEnrollmentReplayExpiryNamesAndCredentials() {
  resetPhone2Fakes();
  SessionManager sessions;
  EnrollmentManager manager(sessions);
  seedOwner();
  char bearer[65] = {};
  check(managementLogin(sessions, manager, "D000001", kOwnerCredential, 1000, bearer),
        "active Owner logs in using independent management bearer");
  check(std::strcmp(bearer, kOwnerCredential) != 0, "management bearer is not device credential");

  char userId[9] = {};
  check(manager.addUser("นภา", "User", userId), "Owner creates Thai named User");
  check(std::strcmp(userId, "U000002") == 0, "new user receives next unique ID");
  char url[128] = {};
  check(manager.createEnrollment(userId, "QR device name", 1001, url), "Owner creates enrollment session");
  const std::string session = sessionFromUrl(url);
  check(session.size() == 64, "enrollment URL carries a 64-character session");
  String info;
  check(manager.enrollmentInfo(session.c_str(), 1002, info), "phone can read pending enrollment info");
  check(info.stdString().find("นภา") != std::string::npos, "enrollment info preserves UTF-8 Thai name");

  char deviceId[9] = {};
  check(manager.completeEnrollment(session.c_str(), kPhoneOneCredential, "มือถือ", 1003, deviceId),
        "phone completes enrollment with its submitted name");
  check(std::strcmp(deviceId, "D000002") == 0, "first phone gets a distinct device ID");
  const char* savedName = nullptr;
  const char* savedUser = nullptr;
  check(fakeDevice(deviceId, &savedUser, &savedName), "enrolled phone appears in fake store");
  check(std::strcmp(savedUser, userId) == 0, "enrolled phone is bound to intended user");
  check(std::strcmp(savedName, "มือถือ") == 0, "stored phone name comes from completion request");
  check(!manager.completeEnrollment(session.c_str(), kPhoneTwoCredential, "Replay", 1004, deviceId),
        "consumed enrollment session cannot be replayed");
  check(AuthStore::verifyDevice("D000002", kPhoneOneCredential), "first device credential verifies only for first device");
  check(!AuthStore::verifyDevice("D000002", kPhoneTwoCredential), "different device credential does not verify as first device");

  check(manager.createEnrollment(userId, "QR second device", 1500, url), "second enrollment is created");
  const std::string second = sessionFromUrl(url);
  check(manager.completeEnrollment(second.c_str(), kPhoneTwoCredential, "เครื่องสอง", 1501, deviceId),
        "second phone completes with an independent credential");
  check(std::strcmp(deviceId, "D000003") == 0, "second phone receives a new device ID");
  check(AuthStore::verifyDevice("D000002", kPhoneOneCredential) &&
        AuthStore::verifyDevice("D000003", kPhoneTwoCredential), "both device credentials verify independently");
  check(!AuthStore::verifyDevice("D000002", kPhoneTwoCredential) &&
        !AuthStore::verifyDevice("D000003", kPhoneOneCredential), "credentials cannot cross-authorize devices");

  check(manager.createEnrollment(userId, "expires", 2000, url), "expiry enrollment is created");
  const std::string expired = sessionFromUrl(url);
  check(!manager.enrollmentInfo(expired.c_str(), 122000, info), "enrollment info rejects expired session");
  check(!manager.completeEnrollment(expired.c_str(), kPhoneTwoCredential, "Expired phone", 122000, deviceId),
        "expired enrollment cannot add a device");
  check(fakeDeviceCount() == 3, "replay and expiry leave device count unchanged");
}

void testProductionSessionBoundaries() {
  SessionManager sessions;
  char management[65] = {};
  char access[65] = {};
  check(!sessions.createSession(SessionType::Setup, 0, 10, management), "production session manager rejects zero TTL");
  check(!sessions.createSession(SessionType::Setup, 300001, 10, management), "production session manager caps TTL at five minutes");
  check(sessions.createSession(SessionType::Management, 1000, 10, management), "production manager creates management token");
  check(sessions.createSession(SessionType::Access, 1000, 10, access), "production manager creates access token");
  check(std::strcmp(management, access) != 0, "independent session types receive independent bearer values");
  check(!sessions.validateSession(SessionType::Access, management, 11), "management token cannot satisfy Access session type");
  check(sessions.validateSession(SessionType::Management, management, 1009), "session is valid before expiry boundary");
  check(!sessions.validateSession(SessionType::Management, management, 1010), "session expires at TTL boundary");
  check(sessions.consumeSession(SessionType::Access, access, 11), "production access session can be consumed once");
  check(!sessions.consumeSession(SessionType::Access, access, 12), "consumed production session cannot be reused");
}

void testRoleRulesAndIssuerRevocation() {
  resetPhone2Fakes();
  SessionManager sessions;
  EnrollmentManager manager(sessions);
  seedOwner();
  char ownerBearer[65] = {};
  check(managementLogin(sessions, manager, "D000001", kOwnerCredential, 100, ownerBearer), "Owner session established");
  char adminUser[9] = {};
  check(manager.addUser("Admin", "Admin", adminUser), "Owner can add Admin");
  char url[128] = {};
  check(manager.createEnrollment(adminUser, "Admin device", 101, url), "Owner can enroll Admin");
  char adminDevice[9] = {};
  std::string adminSession = sessionFromUrl(url);
  check(manager.completeEnrollment(adminSession.c_str(), kAdminCredential, "Admin phone", 102, adminDevice),
        "Admin enrollment completes");

  char adminBearer[65] = {};
  check(managementLogin(sessions, manager, adminDevice, kAdminCredential, 200, adminBearer), "active Admin logs in");
  check(std::strcmp(manager.actorRole(), "Admin") == 0, "actorRole reports authenticated Admin");
  char ignoredId[9] = {};
  check(!manager.addUser("No escalations", "Admin", ignoredId), "Admin cannot create another Admin");
  char targetUser[9] = {};
  check(manager.addUser("Somchai", "User", targetUser), "Admin can add ordinary User");
  check(!manager.createEnrollment("U000001", "Owner device", 201, url), "Admin cannot enroll a device for Owner");
  check(manager.createEnrollment(targetUser, "Pending", 202, url), "Admin can grant User enrollment");
  const std::string pending = sessionFromUrl(url);

  check(managementLogin(sessions, manager, "D000001", kOwnerCredential, 300, ownerBearer),
        "Owner can take over management actor");
  check(manager.setUserEnabled(adminUser, false), "Owner can disable Admin account");
  String canceledInfo;
  check(!manager.enrollmentInfo(pending.c_str(), 301, canceledInfo),
        "disabling grant issuer invalidates its pending enrollment session");
  char phone[9] = {};
  check(!manager.completeEnrollment(pending.c_str(), kPhoneOneCredential, "Canceled", 302, phone),
        "revoked issuer cannot finish an outstanding enrollment");
  check(!manager.setUserEnabled("U000001", false), "Owner account cannot be disabled");
  check(!manager.revoke("D000001"), "primary Owner device cannot be revoked");
}

void testAccessByRoleLanAndRevocation() {
  resetPhone2Fakes();
  SessionManager sessions;
  EnrollmentManager enrollment(sessions);
  seedOwner();
  addUserRecord("U000002", "Admin", UserRole::Admin);
  addDeviceRecord("D000002", "U000002", "Admin phone", kAdminCredential);
  addUserRecord("U000003", "Napa", UserRole::User);
  addDeviceRecord("D000003", "U000003", "User phone", kPhoneOneCredential);
  addUserRecord("U000004", "Guest", UserRole::Guest);
  addDeviceRecord("D000004", "U000004", "Guest phone", kPhoneTwoCredential);
  char deniedBearer[65] = {};
  check(!managementLogin(sessions, enrollment, "D000003", kPhoneOneCredential, 0, deniedBearer),
        "User cannot establish a management session");
  check(!managementLogin(sessions, enrollment, "D000004", kPhoneTwoCredential, 0, deniedBearer),
        "Guest cannot establish a management session");
  ConfigStore config;
  LockController lock;
  AccessController access(sessions, lock, config, enrollment);
  uint32_t duration = 0;

  auto request = [&](const char* device, const char* credential, bool homeLan) {
    char qr[65] = {};
    check(sessions.createSession(SessionType::Access, 5000, millis(), qr), "fresh access QR created");
    lock.locked = true;
    return access.request(qr, device, credential, millis(), duration, homeLan);
  };

  check(request("D000001", kOwnerCredential, false) == AccessController::Result::Unlocked,
        "Owner can unlock on setup AP");
  check(std::strcmp(Audit::lastUser(), "U000001") == 0, "Owner grant audit records actual user ID");
  check(request("D000002", kAdminCredential, false) == AccessController::Result::Denied,
        "Admin is denied on setup AP");
  check(request("D000003", kPhoneOneCredential, false) == AccessController::Result::Denied,
        "User is denied on setup AP");
  check(request("D000004", kPhoneTwoCredential, false) == AccessController::Result::Denied,
        "Guest is denied on setup AP");
  check(request("D000002", kAdminCredential, true) == AccessController::Result::Unlocked,
        "Admin can unlock on home LAN");
  check(std::strcmp(Audit::lastUser(), "U000002") == 0, "Admin grant audit uses Admin user ID");
  check(request("D000003", kPhoneOneCredential, true) == AccessController::Result::Unlocked,
        "User can unlock on home LAN");
  check(std::strcmp(Audit::lastUser(), "U000003") == 0, "User grant audit uses User ID");
  check(request("D000004", kPhoneTwoCredential, true) == AccessController::Result::Unlocked,
        "Guest can unlock on home LAN");
  check(std::strcmp(Audit::lastUser(), "U000004") == 0, "Guest grant audit uses Guest ID");
  check(duration == 5000 && lock.lastDuration == 5000, "configured duration reaches fake lock");

  // Unknown enum values are represented in the isolated fake store to verify fail-closed access.
  UserRecord users[UserStore::kMaxRecords] = {};
  size_t uc = 0;
  check(UserStore::load(users, UserStore::kMaxRecords, uc), "users load before unknown-role case");
  std::snprintf(users[uc].id, sizeof(users[uc].id), "U000005");
  std::snprintf(users[uc].name, sizeof(users[uc].name), "Unknown");
  users[uc].role = static_cast<UserRole>(99); users[uc].status = RecordStatus::Active;
  check(UserStore::replace(users, ++uc), "unknown role inserted into fake store");
  addDeviceRecord("D000005", "U000005", "Unknown phone", "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee");
  check(request("D000005", "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee", true) ==
        AccessController::Result::Denied, "unknown role is denied even on home LAN");

  char ownerBearer[65] = {};
  check(managementLogin(sessions, enrollment, "D000001", kOwnerCredential, 500, ownerBearer),
        "Owner can authenticate to manage users and devices");
  check(enrollment.setUserEnabled("U000003", false), "Owner disables a User account");
  check(request("D000003", kPhoneOneCredential, true) == AccessController::Result::Denied,
        "disabled User cannot unlock over home LAN");
  check(enrollment.revoke("D000002"), "Owner revokes an Admin device");
  check(request("D000002", kAdminCredential, true) == AccessController::Result::Denied,
        "revoked Admin device cannot unlock over home LAN");

  char rejectedToken[65] = {};
  check(enrollment.login("not-a-session", "D000001", kOwnerCredential, millis(), rejectedToken) == false,
        "consumed or unknown login QR cannot establish a management actor");
}

void testRevocationAcrossUserReenableAndControllerRecreation() {
  resetPhone2Fakes();
  SessionManager firstSessions;
  EnrollmentManager firstManager(firstSessions);
  seedOwner();
  addUserRecord("U000002", "Napa", UserRole::User);
  addDeviceRecord("D000002", "U000002", "User phone", kPhoneOneCredential);
  char ownerBearer[65] = {};
  check(managementLogin(firstSessions, firstManager, "D000001", kOwnerCredential, 700, ownerBearer),
        "Owner logs in before device revocation scenario");
  check(firstManager.setUserEnabled("U000002", false), "Owner disables User before revoking its device");
  check(firstManager.revoke("D000002"), "Owner revokes individual User device");
  check(firstManager.setUserEnabled("U000002", true), "Owner reenables the User account");

  auto accessCheck = [&](SessionManager& sessions, EnrollmentManager& enrollment,
                         LockController& lock, ConfigStore& config,
                         const char* device, const char* credential, bool homeLan) {
    AccessController access(sessions, lock, config, enrollment);
    char qr[65] = {};
    setFakeMillis(800);
    check(sessions.createSession(SessionType::Access, 30000, 800, qr), "post-change access QR created");
    lock.locked = true;
    uint32_t duration = 0;
    return access.request(qr, device, credential, 800, duration, homeLan);
  };
  LockController firstLock;
  ConfigStore firstConfig;
  check(accessCheck(firstSessions, firstManager, firstLock, firstConfig,
                    "D000002", kPhoneOneCredential, true) == AccessController::Result::Denied,
        "reenabling User does not restore its individually revoked device");

  // Controller recreation keeps only fake-store state; process-local manager/session state is fresh.
  SessionManager restartedSessions;
  EnrollmentManager restartedManager(restartedSessions);
  LockController restartedLock;
  ConfigStore restartedConfig;
  check(accessCheck(restartedSessions, restartedManager, restartedLock, restartedConfig,
                    "D000002", kPhoneOneCredential, true) == AccessController::Result::Denied,
        "revoked D000002 remains denied after controller recreation");
  check(accessCheck(restartedSessions, restartedManager, restartedLock, restartedConfig,
                    "D000001", kOwnerCredential, false) == AccessController::Result::Unlocked,
        "Owner D000001 remains valid after controller recreation");
}

void testAccessFailureBoundaries() {
  resetPhone2Fakes();
  seedOwner();
  SessionManager sessions;
  EnrollmentManager enrollment(sessions);
  ConfigStore config;
  LockController lock;
  AccessController access(sessions, lock, config, enrollment);
  auto request = [&](uint32_t now) {
    char qr[65] = {};
    setFakeMillis(now);
    check(sessions.createSession(SessionType::Access, 30000, now, qr), "failure case gets fresh Access session");
    lock.locked = true;
    uint32_t duration = 0;
    return access.request(qr, "D000001", kOwnerCredential, now, duration, false);
  };

  const unsigned initialUnlockCalls = lock.unlockCalls;
  config.setHealthy(false);
  check(request(10) == AccessController::Result::StorageFault, "unhealthy config storage rejects access");
  config.setHealthy(true);
  config.mutableConfig().configured = false;
  check(request(20) == AccessController::Result::StorageFault, "unconfigured device rejects access");
  config.mutableConfig().configured = true;
  config.mutableConfig().ownerExists = false;
  check(request(30) == AccessController::Result::StorageFault, "missing Owner state rejects access");
  config.mutableConfig().ownerExists = true;
  config.mutableConfig().unlockDurationMs = 999;
  check(request(40) == AccessController::Result::StorageFault, "unlock duration below minimum rejects access");
  config.mutableConfig().unlockDurationMs = 60001;
  check(request(50) == AccessController::Result::StorageFault, "unlock duration above maximum rejects access");
  config.mutableConfig().unlockDurationMs = 5000;
  check(lock.unlockCalls == initialUnlockCalls, "invalid configuration never calls fake lock output");

  lock.nextUnlockResult = LockController::UnlockResult::TimerUnavailable;
  check(request(60) == AccessController::Result::Denied, "fake lock timer failure is denied");
  lock.nextUnlockResult = LockController::UnlockResult::InvalidDuration;
  check(request(70) == AccessController::Result::Denied, "fake lock invalid duration is denied");

  lock.nextUnlockResult = LockController::UnlockResult::Unlocked;
  setPhone2StoreFailures(true, false, false);
  check(request(80) == AccessController::Result::StorageFault, "UserStore read failure denies access as storage fault");
  setPhone2StoreFailures(false, true, false);
  check(request(90) == AccessController::Result::StorageFault, "DeviceStore read failure denies access as storage fault");
  setPhone2StoreFailures(false, false, true);
  check(request(100) == AccessController::Result::StorageFault, "AuthStore integrity failure denies access as storage fault");
  setPhone2StoreFailures(false, false, false);
}
}

int main() {
  testProductionSessionBoundaries();
  testEnrollmentReplayExpiryNamesAndCredentials();
  testRoleRulesAndIssuerRevocation();
  testAccessByRoleLanAndRevocation();
  testRevocationAcrossUserReenableAndControllerRecreation();
  testAccessFailureBoundaries();
  std::cout << "PASS: " << checks << " assertions across Phone2 enrollment, roles, and access groups\n";
}
