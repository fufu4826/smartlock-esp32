#include "Audit.h"
#include "SD.h"
#include "Preferences.h"
#include "Arduino.h"
#include "WiFi.h"
#include "TimeManager.h"
#include "../../src/storage/IdentityStore.h"
#include "../../src/storage/AuthStore.h"
#include <cassert>
#include <cstring>
#include <string>
#include <cstdio>

uint32_t mockMillis = 0;
SerialClass Serial;
ESPClass ESP;
WiFiClass WiFi;
SDClass SD;
void esp_fill_random(void* output, size_t length) {
  static uint8_t next = 1;
  auto* bytes = static_cast<uint8_t*>(output);
  for (size_t i = 0; i < length; ++i) bytes[i] = next++;
}

uint32_t TimeManager::epochSeconds() { return 0; }
bool TimeManager::synchronized() { return false; }
void TimeManager::update(bool, uint32_t) {}
namespace smartlock { namespace storage {
bool IdentityStore::load(IdentityRecord*, size_t, size_t& count) { count = 0; return true; }
char mockOwnerName[41] = "Owner, \"At Setup\" =";
bool IdentityStore::find(const char* id, IdentityRecord& output) {
  if (!id || (std::strcmp(id, "D000001") && std::strcmp(id, "D000002"))) return false;
  std::memset(&output, 0, sizeof(output));
  std::snprintf(output.id, sizeof(output.id), "%s", id);
  std::snprintf(output.name, sizeof(output.name), "%s", !std::strcmp(id, "D000001") ? mockOwnerName : "Revoked, \"Phone\"");
  output.role = !std::strcmp(id, "D000001") ? UserRole::Owner : UserRole::Admin;
  output.status = !std::strcmp(id, "D000001") ? RecordStatus::Active : RecordStatus::Revoked;
  return true;
}
bool IdentityStore::backupVerifierUnchanged() { return true; }
} }
bool AuthStore::hasDevice(const char*) { return false; }
AuthStore::ReadResult AuthStore::inspect(size_t& count) { count = 0; return ReadResult::Valid; }

int main() {
  SD.clear(); Preferences::clearAll();
  assert(Audit::begin(true));
  mockMillis = 50;
  Audit::granted("U000001", "D000001", smartlock::events::Source::Lan);
  std::snprintf(smartlock::storage::mockOwnerName, sizeof(smartlock::storage::mockOwnerName), "%s", "Renamed Later");
  Audit::denied("D000002", "D000002", smartlock::events::Source::RecoveryAp);
  Audit::denied("D999999", "", smartlock::events::Source::Lan);
  Audit::denied("credential-secret", "", smartlock::events::Source::Lan);
  Audit::record(smartlock::events::Action::DeviceEnrolled, smartlock::events::Result::Success,
                "", "D000002", smartlock::events::Source::Lan);
  Audit::record(smartlock::events::Action::DeviceRevoked, smartlock::events::Result::Success,
                "", "D000002", smartlock::events::Source::Lan);
  Audit::emergencyGranted();
  for (uint32_t tick = 100; tick <= 900; tick += 100) {
    mockMillis = tick;
    Audit::update(tick >= 800, true, tick);
  }
  for (uint32_t tick = 1000; tick <= 1200; tick += 100) {
    mockMillis = tick;
    Audit::update(true, true, tick);
  }
  auto* log = Audit::log();
  assert(log && log->sealCurrentSegment());
  smartlock::events::EventLog::SegmentInfo info[smartlock::events::EventLog::kMaxSegments] = {};
  size_t count = 0;
  const bool enumerated = log->enumerateSegments(info, smartlock::events::EventLog::kMaxSegments, count);
  assert(enumerated && count == 1);
  char csv[smartlock::events::EventLog::kSegmentBufferCapacity] = {};
  size_t length = 0;
  assert(log->readSegmentById(info[0].id, csv, sizeof(csv), length));
  const std::string events(csv, length);
  assert(events.find(",D000001,ACCESS_GRANTED,SUCCESS,AP_STA,50,,LAN,") != std::string::npos);
  assert(events.find(",D000002,ACCESS_DENIED,DENIED,AP_STA,50,D000002,RECOVERY_AP,") != std::string::npos);
  assert(events.find(",,ACCESS_DENIED,DENIED,AP_STA,50,D999999,LAN,") != std::string::npos);
  assert(events.find("EMERGENCY_UNLOCK_ADMIN_PIN,SUCCESS") != std::string::npos);
  assert(events.find("EMERGENCY_RELOCK,SUCCESS") != std::string::npos);
  assert(events.find("123456") == std::string::npos);
  assert(events.find("secret") == std::string::npos);
  const std::string originalSnapshot = "3,\"Owner, \"\"At Setup\"\" =\",OWNER,ACTOR,D000001,,google-v1";
  assert(events.find(originalSnapshot) != std::string::npos);
  assert(events.find("3,\"Revoked, \"\"Phone\"\"\",ADMIN,ACTOR,D000002,,google-v1") != std::string::npos);
  assert(events.find(",D999999,LAN,3,\"\",,UNKNOWN,,,") != std::string::npos);
  assert(events.find(",DEVICE_ENROLLED,SUCCESS") != std::string::npos &&
         events.find("3,\"Revoked, \"\"Phone\"\"\",ADMIN,SUBJECT,,D000002,google-v1") != std::string::npos);
  assert(events.find(",EMERGENCY_UNLOCK_ADMIN_PIN,SUCCESS,AP_STA,50,,PHYSICAL_ADMIN,3,\"\",,UNKNOWN,,,google-v1") != std::string::npos);
  return 0;
}
