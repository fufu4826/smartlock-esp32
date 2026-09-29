#include "phone2_test_support.h"

#include <string.h>
#include <stdint.h>
#include <esp_system.h>

using namespace smartlock::storage;
namespace {
uint32_t fakeNow = 0;
UserRecord users[UserStore::kMaxRecords] = {};
size_t usersCount = 0;
DeviceRecord devices[DeviceStore::kMaxRecords] = {};
size_t devicesCount = 0;
struct Credential { char id[9]; char value[65]; } credentials[64] = {};
size_t credentialsCount = 0;
char auditedUser[9] = {};
char auditedDevice[9] = {};
unsigned eventCounter = 0;
bool failUserLoad = false;
bool failDeviceLoad = false;
bool failAuthInspect = false;

bool sameCredential(const char* id, const char* value) {
  if (!id || !value) return false;
  for (size_t i = 0; i < credentialsCount; ++i)
    if (!strcmp(credentials[i].id, id)) return !strcmp(credentials[i].value, value);
  return false;
}
}

uint32_t millis() { return fakeNow; }
void setFakeMillis(uint32_t nowMs) { fakeNow = nowMs; }

void esp_fill_random(void* output, size_t length) {
  static uint32_t stream = 0x5a17c0deu;
  auto* bytes = static_cast<uint8_t*>(output);
  for (size_t i = 0; i < length; ++i) {
    stream = stream * 1664525u + 1013904223u;
    bytes[i] = static_cast<uint8_t>(stream >> 24);
  }
}

bool UserStore::load(UserRecord* records, size_t capacity, size_t& count) {
  count = usersCount;
  if (failUserLoad) return false;
  if (!records || capacity < usersCount) return false;
  memcpy(records, users, usersCount * sizeof(UserRecord)); return true;
}
bool UserStore::replace(const UserRecord* records, size_t count) {
  if (!records || count > kMaxRecords) return false;
  memcpy(users, records, count * sizeof(UserRecord)); usersCount = count; return true;
}
bool DeviceStore::load(DeviceRecord* records, size_t capacity, size_t& count) {
  count = devicesCount;
  if (failDeviceLoad) return false;
  if (!records || capacity < devicesCount) return false;
  memcpy(records, devices, devicesCount * sizeof(DeviceRecord)); return true;
}
bool DeviceStore::replace(const DeviceRecord* records, size_t count) {
  if (!records || count > kMaxRecords) return false;
  memcpy(devices, records, count * sizeof(DeviceRecord)); devicesCount = count; return true;
}
bool AuthStore::hasDevice(const char* id) {
  if (!id) return false;
  for (size_t i = 0; i < credentialsCount; ++i) if (!strcmp(credentials[i].id, id)) return true;
  return false;
}
bool AuthStore::verifyDevice(const char* id, const char* credential) { return sameCredential(id, credential); }
bool AuthStore::addDevice(const char* id, const char* credential) {
  if (!id || !credential || hasDevice(id) || credentialsCount >= 64) return false;
  snprintf(credentials[credentialsCount].id, 9, "%s", id);
  snprintf(credentials[credentialsCount].value, 65, "%s", credential);
  ++credentialsCount; return true;
}
AuthStore::ReadResult AuthStore::inspect(size_t& count) {
  count = credentialsCount; return failAuthInspect ? ReadResult::Invalid : ReadResult::Valid;
}

const char* CanonicalOrigin::host() { return "smart-lock-test.local"; }
void Audit::record(smartlock::events::Action, smartlock::events::Result,
                   const char* userId, const char* deviceId) {
  snprintf(auditedUser, sizeof(auditedUser), "%s", userId ? userId : "");
  snprintf(auditedDevice, sizeof(auditedDevice), "%s", deviceId ? deviceId : "");
  ++eventCounter;
}
void Audit::granted(const char* userId, const char* deviceId) {
  record(smartlock::events::Action::AccessGranted, smartlock::events::Result::Success, userId, deviceId);
}
const char* Audit::lastUser() { return auditedUser; }
const char* Audit::lastDevice() { return auditedDevice; }

void resetPhone2Fakes() {
  fakeNow = 0; memset(users, 0, sizeof(users)); usersCount = 0;
  memset(devices, 0, sizeof(devices)); devicesCount = 0;
  memset(credentials, 0, sizeof(credentials)); credentialsCount = 0;
  memset(auditedUser, 0, sizeof(auditedUser)); memset(auditedDevice, 0, sizeof(auditedDevice)); eventCounter = 0;
  failUserLoad = failDeviceLoad = failAuthInspect = false;
}
void setPhone2StoreFailures(bool userLoad, bool deviceLoad, bool authInspect) {
  failUserLoad = userLoad; failDeviceLoad = deviceLoad; failAuthInspect = authInspect;
}
bool fakeUser(const char* id, UserRole* role, RecordStatus* status, const char** name) {
  for (size_t i = 0; i < usersCount; ++i) if (!strcmp(users[i].id, id)) {
    if (role) *role = users[i].role; if (status) *status = users[i].status; if (name) *name = users[i].name; return true;
  }
  return false;
}
bool fakeDevice(const char* id, const char** userId, const char** name, RecordStatus* status) {
  for (size_t i = 0; i < devicesCount; ++i) if (!strcmp(devices[i].id, id)) {
    if (userId) *userId = devices[i].userId; if (name) *name = devices[i].name; if (status) *status = devices[i].status; return true;
  }
  return false;
}
size_t fakeDeviceCount() { return devicesCount; }
size_t fakeUserCount() { return usersCount; }
