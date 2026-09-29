#include "Audit.h"
#include "TimeManager.h"
#include <Arduino.h>
#include <WiFi.h>
#include <new>
#include <memory>
#include <string.h>
#include "../storage/IdentityStore.h"
#include "../storage/AuthStore.h"
#include <driver/gpio.h>

namespace {
smartlock::events::EventLog* eventLog = nullptr;
bool awaitingRelock = false;
bool awaitingEmergencyRelock = false;
void (*systemDiagnostic)() = nullptr;
char unlockedUser[8] = {};
char unlockedDevice[8] = {};
char unlockedName[41] = {};
char unlockedRole[6] = {};
char unlockedActorId[8] = {};
smartlock::events::Source unlockedSource = smartlock::events::Source::Unknown;
uint32_t lastDrain = 0;
const char* safeDeviceId(const char* value) {
  if (!value || strnlen(value, 8) != 7 || value[0] != 'D') return "";
  for (size_t i = 1; i < 7; ++i) if (value[i] < '0' || value[i] > '9') return "";
  return value;
}
const char* roleName(smartlock::storage::UserRole role) {
  using smartlock::storage::UserRole;
  switch (role) {
    case UserRole::Owner: return "OWNER";
    case UserRole::Admin: return "ADMIN";
    case UserRole::User: return "USER";
    case UserRole::Guest: return "GUEST";
  }
  return "";
}
void recordSnapshot(smartlock::events::Action action, smartlock::events::Result result,
                    const char* userId, const char* deviceId,
                    smartlock::events::Source source, const char* claimedDeviceId,
                    const char* name, const char* role, const char* context,
                    const char* actorId, const char* subjectId) {
  if (!eventLog) return;
  const auto mode = WiFi.getMode() == WIFI_STA ? smartlock::events::NetworkMode::STA : WiFi.getMode() == WIFI_AP_STA ? smartlock::events::NetworkMode::AP_STA
                                                : smartlock::events::NetworkMode::AP;
  eventLog->enqueue(action, result, userId, deviceId,
                    TimeManager::epochSeconds(), millis(), mode, source,
                    claimedDeviceId, name, role, context, actorId, subjectId);
}
bool snapshotFor(const char* deviceId, char (&name)[41], char (&role)[6]) {
  name[0] = role[0] = '\0';
  if (!deviceId || !*deviceId) return false;
  smartlock::storage::IdentityRecord identity = {};
  if (!smartlock::storage::IdentityStore::find(deviceId, identity)) return false;
  snprintf(name, sizeof(name), "%s", identity.name);
  snprintf(role, sizeof(role), "%s", roleName(identity.role));
  return true;
}
}

bool Audit::begin(bool sdAvailable) {
  if (!eventLog) eventLog = new (std::nothrow) smartlock::events::EventLog;
  return eventLog && eventLog->begin(sdAvailable);
}
smartlock::events::EventLog* Audit::log() { return eventLog; }

void Audit::record(smartlock::events::Action action, smartlock::events::Result result,
                   const char* userId, const char* deviceId,
                   smartlock::events::Source source, const char* claimedDeviceId) {
  char name[41] = {}, role[6] = {}, actorId[8] = {}, subjectId[8] = {};
  const char* context = "UNKNOWN";
  const char* snapshotId = "";
  if (source != smartlock::events::Source::PhysicalAdmin) {
    if (action == smartlock::events::Action::DeviceEnrolled ||
        action == smartlock::events::Action::DeviceRevoked) {
      snapshotId = safeDeviceId(deviceId);
      if (snapshotId[0]) { memcpy(subjectId, snapshotId, 8); context = "SUBJECT"; }
    } else if (action == smartlock::events::Action::AccessGranted ||
               action == smartlock::events::Action::AccessDenied ||
               action == smartlock::events::Action::Unlock ||
               action == smartlock::events::Action::Relock ||
               action == smartlock::events::Action::AdminLogin) {
      snapshotId = safeDeviceId(deviceId);
      if (snapshotId[0]) { memcpy(actorId, snapshotId, 8); context = "ACTOR"; }
    }
    if (snapshotId[0]) snapshotFor(snapshotId, name, role);
  }
  recordSnapshot(action, result, userId, deviceId, source, claimedDeviceId,
                 name, role, context, actorId, subjectId);
}

void Audit::denied(const char* claimedDeviceId, const char* verifiedDeviceId,
                   smartlock::events::Source source,
                   smartlock::events::Result result) {
  record(smartlock::events::Action::AccessDenied, result, "",
         safeDeviceId(verifiedDeviceId), source, safeDeviceId(claimedDeviceId));
}

void Audit::granted(const char* userId, const char* deviceId,
                    smartlock::events::Source source) {
  // New events use an empty legacy user_id and the verified identity in device_id.
  snprintf(unlockedUser, sizeof(unlockedUser), "%s", userId);
  snprintf(unlockedDevice, sizeof(unlockedDevice), "%s", deviceId);
  unlockedSource = source;
  awaitingRelock = true;
  unlockedName[0] = unlockedRole[0] = unlockedActorId[0] = '\0';
  if (source != smartlock::events::Source::PhysicalAdmin && snapshotFor(safeDeviceId(deviceId), unlockedName, unlockedRole))
    snprintf(unlockedActorId, sizeof(unlockedActorId), "%s", safeDeviceId(deviceId));
  recordSnapshot(smartlock::events::Action::AccessGranted, smartlock::events::Result::Success,
         unlockedUser, unlockedDevice, source, "", unlockedName, unlockedRole,
         unlockedActorId[0] ? "ACTOR" : "UNKNOWN", unlockedActorId, "");
  recordSnapshot(smartlock::events::Action::Unlock, smartlock::events::Result::Success,
         unlockedUser, unlockedDevice, source, "", unlockedName, unlockedRole,
         unlockedActorId[0] ? "ACTOR" : "UNKNOWN", unlockedActorId, "");
}

void Audit::emergencyGranted() {
  awaitingEmergencyRelock = true;
  record(smartlock::events::Action::EmergencyUnlockAdminPin,
         smartlock::events::Result::Success, "", "",
         smartlock::events::Source::PhysicalAdmin);
}

void Audit::setSystemDiagnostic(void (*callback)()) {
  systemDiagnostic = callback;
}

void Audit::update(bool locked, bool staConnected, uint32_t nowMs) {
  TimeManager::update(staConnected, nowMs);
  if (awaitingRelock && locked) {
    recordSnapshot(smartlock::events::Action::Relock, smartlock::events::Result::Success,
           unlockedUser, unlockedDevice, unlockedSource, "", unlockedName, unlockedRole,
           unlockedActorId[0] ? "ACTOR" : "UNKNOWN", unlockedActorId, "");
    awaitingRelock = false;
    memset(unlockedUser, 0, sizeof(unlockedUser));
    memset(unlockedDevice, 0, sizeof(unlockedDevice));
    memset(unlockedName, 0, sizeof(unlockedName));
    memset(unlockedRole, 0, sizeof(unlockedRole));
    memset(unlockedActorId, 0, sizeof(unlockedActorId));
    unlockedSource = smartlock::events::Source::Unknown;
  }
  if (awaitingEmergencyRelock && locked) {
    record(smartlock::events::Action::EmergencyRelock,
           smartlock::events::Result::Success, "", "",
           smartlock::events::Source::PhysicalAdmin);
    awaitingEmergencyRelock = false;
  }
  if (eventLog && static_cast<uint32_t>(nowMs - lastDrain) >= 100) {
    lastDrain = nowMs;
    eventLog->update();
  }
}

void Audit::pollDiagnostics() {
  // USB-only, fixed read-only diagnostic. No auth record/credential output,
  // arbitrary filename, reset, configuration write or unlock command exists.
  static char command[24] = {};
  static uint8_t used = 0;
  static bool overflow = false;
  for (uint8_t budget = 0; budget < 32 && Serial.available(); ++budget) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c != '\n') {
      if (used < sizeof(command) - 1) command[used++] = c;
      else overflow = true;
      continue;
    }
    command[used] = '\0';
    const bool authRequested = !overflow && !strcmp(command, "DIAG_AUTH");
    const bool requested = !overflow && !strcmp(command, "DIAG_AUDIT");
    const bool systemRequested = !overflow && !strcmp(command, "DIAG_SYSTEM");
    used = 0; overflow = false;
    if (authRequested) {
      using namespace smartlock::storage;
      std::unique_ptr<IdentityRecord[]> rows(new(std::nothrow) IdentityRecord[IdentityStore::kMaxRecords]);
      size_t count=0,auth=0,owners=0;
      const bool ok=rows && IdentityStore::load(rows.get(),IdentityStore::kMaxRecords,count) &&
          AuthStore::inspect(auth)==AuthStore::ReadResult::Valid;
      if(ok) for(size_t i=0;i<count;++i) {
        if(rows[i].role==UserRole::Owner)++owners;
        Serial.printf("IDENTITY META: id=%s name=%s role=%u status=%u verifier=%u\n",rows[i].id,
            rows[i].name,(unsigned)rows[i].role,(unsigned)rows[i].status,AuthStore::hasDevice(rows[i].id));
      }
      Serial.printf("AUTH INSPECT: valid=%u identities=%u owners=%u verifiers=%u OWNER_VERIFIER_PRESERVED=%u heap=%u min_heap=%u\n",
          ok,(unsigned)count,(unsigned)owners,(unsigned)auth,IdentityStore::backupVerifierUnchanged(),
          ESP.getFreeHeap(),ESP.getMinFreeHeap());
      Serial.println("AUTH INSPECT END");
      continue;
    }
    if (systemRequested) {
      if (systemDiagnostic) systemDiagnostic();
      Serial.println("SYSTEM DIAG END");
      continue;
    }
    if (!requested || !eventLog) continue;
    smartlock::events::EventLog::Status status = {}; eventLog->status(status);
    Serial.printf("AUDIT STATUS: enabled=%u fault=%u full=%u queue=%u segments=%u dropped=%u NTP=%u\n",
        status.enabled, status.storageFault, status.outboxFull, status.queueDepth,
        status.segmentCount, static_cast<unsigned>(status.droppedEvents), TimeManager::synchronized());
    std::unique_ptr<smartlock::events::EventLog::SegmentInfo[]> segments(
        new (std::nothrow) smartlock::events::EventLog::SegmentInfo[smartlock::events::EventLog::kMaxSegments]{});
    size_t segmentCount = 0;
    if (segments && eventLog->enumerateSegments(segments.get(), smartlock::events::EventLog::kMaxSegments, segmentCount)) {
      Serial.printf("AUDIT HISTORY: lost_events=%u lost_segments=%u retained=%u oldest=%s newest=%s\n",
          static_cast<unsigned>(status.historyLostEvents), static_cast<unsigned>(status.historyLostSegments),
          static_cast<unsigned>(segmentCount), segmentCount ? segments[0].id : "", segmentCount ? segments[segmentCount-1].id : "");
    } else Serial.println("AUDIT HISTORY: unavailable");
    eventLog->sealCurrentSegment();
    char basename[smartlock::events::EventLog::kBasenameCapacity] = {};
    std::unique_ptr<char[]> bytes(new (std::nothrow) char[smartlock::events::EventLog::kSegmentBufferCapacity]);
    size_t length = 0;
    if (bytes && eventLog->selectOldestClosedSegment(basename, sizeof(basename)) &&
        eventLog->readClosedSegment(basename, bytes.get(),
                       smartlock::events::EventLog::kSegmentBufferCapacity, length)) {
      Serial.println("AUDIT CSV BEGIN");
      Serial.write(reinterpret_cast<const uint8_t*>(bytes.get()), length);
      Serial.println("AUDIT CSV END");
    }
    // Read-only named traversal diagnostics use the same validator as export.
    if (bytes && segments && segmentCount > 1) {
      const size_t indices[] = {segmentCount / 2, segmentCount - 1};
      for (size_t i = 0; i < 2; ++i) {
        if (i && indices[i] == indices[i-1]) continue;
        if (eventLog->readSegmentById(segments[indices[i]].id, bytes.get(),
              smartlock::events::EventLog::kSegmentBufferCapacity, length)) {
          Serial.printf("AUDIT NAMED BEGIN %s\n", segments[indices[i]].id);
          Serial.write(reinterpret_cast<const uint8_t*>(bytes.get()), length);
          Serial.println("AUDIT NAMED END");
        }
      }
    }
  }
}
