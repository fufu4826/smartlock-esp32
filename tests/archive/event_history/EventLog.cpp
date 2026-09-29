#include "EventLog.h"

#include <SD.h>
#include <Preferences.h>
#include <bootloader_random.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace smartlock {
namespace events {
namespace {

constexpr char kOutboxPath[] = "/smartlock/outbox";
constexpr char kCsvHeader[] =
    "event_id,timestamp,time_quality,user_id,device_id,action,result,"
    "network_mode,uptime_ms,claimed_device_id,access_source,event_schema,"
    "identity_name,role,identity_context,actor_identity_id,subject_identity_id,"
    "firmware_version,crc32\n";
constexpr char kCurrentCsvHeader[] =
    "event_id,timestamp,time_quality,user_id,device_id,action,result,"
    "network_mode,uptime_ms,claimed_device_id,access_source,crc32\n";
constexpr char kLegacyCsvHeader[] =
    "event_id,timestamp,time_quality,user_id,device_id,action,result,"
    "network_mode,uptime_ms,crc32\n";
constexpr char kHex[] = "0123456789abcdef";
constexpr size_t kMaxRowBytes = 512;

uint32_t crc32(const uint8_t* bytes, size_t length) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < length; ++i) {
    crc ^= bytes[i];
    for (uint8_t bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

const char* actionName(Action action) {
  switch (action) {
    case Action::Boot: return "BOOT";
    case Action::AccessGranted: return "ACCESS_GRANTED";
    case Action::AccessDenied: return "ACCESS_DENIED";
    case Action::Unlock: return "UNLOCK";
    case Action::Relock: return "RELOCK";
    case Action::AdminLogin: return "ADMIN_LOGIN";
    case Action::DeviceEnrolled: return "DEVICE_ENROLLED";
    case Action::DeviceRevoked: return "DEVICE_REVOKED";
    case Action::UserAdded: return "USER_ADDED";
    case Action::NetworkChanged: return "NETWORK_CHANGED";
    case Action::Recovery: return "RECOVERY";
    case Action::EmergencyUnlockAdminPin: return "EMERGENCY_UNLOCK_ADMIN_PIN";
    case Action::EmergencyRelock: return "EMERGENCY_RELOCK";
  }
  return nullptr;
}

const char* resultName(Result result) {
  switch (result) {
    case Result::Success: return "SUCCESS";
    case Result::Denied: return "DENIED";
    case Result::StorageFault: return "STORAGE_FAULT";
  }
  return nullptr;
}

const char* sourceName(Source source) {
  switch (source) {
    case Source::Unknown: return "UNKNOWN";
    case Source::Lan: return "LAN";
    case Source::RecoveryAp: return "RECOVERY_AP";
    case Source::PhysicalAdmin: return "PHYSICAL_ADMIN";
  }
  return nullptr;
}

bool validId(const char* value, char prefix) {
  if (value == nullptr) return false;
  const size_t length = strnlen(value, 8);
  if (length == 0) return true;
  if (length != 7 || value[0] != prefix) return false;
  for (size_t i = 1; i < 7; ++i)
    if (value[i] < '0' || value[i] > '9') return false;
  return true;
}

void copyId(char (&output)[8], const char* input) {
  memset(output, 0, sizeof(output));
  if (input != nullptr && input[0] != '\0') memcpy(output, input, 7);
}

bool validName(const char* value) {
  if (value == nullptr) return false;
  const size_t n = strnlen(value, 41);
  if (n > 40) return false;
  for (size_t i = 0; i < n; ++i) {
    const uint8_t c = static_cast<uint8_t>(value[i]);
    if (c < 0x20 || c == 0x7f) return false;
  }
  return true;
}

bool validRole(const char* value) {
  return value && (!strcmp(value, "") || !strcmp(value, "OWNER") ||
      !strcmp(value, "ADMIN") || !strcmp(value, "USER") || !strcmp(value, "GUEST"));
}

bool validContext(const char* value) {
  return value && (!strcmp(value, "UNKNOWN") || !strcmp(value, "ACTOR") ||
      !strcmp(value, "SUBJECT"));
}

bool validActionField(const char* value, size_t length) {
  static const char* const names[] = {
      "BOOT", "ACCESS_GRANTED", "ACCESS_DENIED", "UNLOCK", "RELOCK",
      "ADMIN_LOGIN", "DEVICE_ENROLLED", "DEVICE_REVOKED", "USER_ADDED",
      "NETWORK_CHANGED", "RECOVERY", "EMERGENCY_UNLOCK_ADMIN_PIN",
      "EMERGENCY_RELOCK"};
  for (const char* name : names)
    if (strlen(name) == length && memcmp(value, name, length) == 0) return true;
  return false;
}

bool validResultField(const char* value, size_t length) {
  return (length == 7 && memcmp(value, "SUCCESS", 7) == 0) ||
         (length == 6 && memcmp(value, "DENIED", 6) == 0) ||
         (length == 13 && memcmp(value, "STORAGE_FAULT", 13) == 0);
}

bool validIdField(const char* value, size_t length, char prefix) {
  if (length == 0) return true;
  if (length != 7 || value[0] != prefix) return false;
  for (size_t i = 1; i < length; ++i)
    if (value[i] < '0' || value[i] > '9') return false;
  return true;
}

bool validCsvName(const char* value, size_t length) {
  if (length < 2 || value[0] != '"' || value[length - 1] != '"') return false;
  size_t decoded = 0;
  for (size_t i = 1; i + 1 < length; ++i) {
    const unsigned char c = static_cast<unsigned char>(value[i]);
    if (c == '"') {
      if (i + 1 >= length - 1 || value[i + 1] != '"') return false;
      ++i;
    } else if (c < 0x20 || c == 0x7f) return false;
    ++decoded;
    if (decoded > 40) return false;
  }
  return true;
}

bool validRoleField(const char* value, size_t length) {
  return (length == 0) || (length == 5 && memcmp(value, "OWNER", 5) == 0) ||
      (length == 5 && memcmp(value, "ADMIN", 5) == 0) ||
      (length == 4 && memcmp(value, "USER", 4) == 0) ||
      (length == 5 && memcmp(value, "GUEST", 5) == 0);
}

bool validContextField(const char* value, size_t length) {
  return (length == 7 && memcmp(value, "UNKNOWN", 7) == 0) ||
      (length == 5 && memcmp(value, "ACTOR", 5) == 0) ||
      (length == 7 && memcmp(value, "SUBJECT", 7) == 0);
}

bool validTimestamp(const char* value, size_t length) {
  if (length == 0) return true;
  if (length != 20 || value[4] != '-' || value[7] != '-' ||
      value[10] != 'T' || value[13] != ':' || value[16] != ':' ||
      value[19] != 'Z') return false;
  for (size_t i = 0; i < length; ++i) {
    if (i == 4 || i == 7 || i == 10 || i == 13 || i == 16 || i == 19)
      continue;
    if (value[i] < '0' || value[i] > '9') return false;
  }
  return true;
}

bool validUptime(const char* value, size_t length) {
  if (length == 0 || length > 10) return false;
  uint64_t parsed = 0;
  for (size_t i = 0; i < length; ++i) {
    if (value[i] < '0' || value[i] > '9') return false;
    parsed = parsed * 10 + static_cast<uint8_t>(value[i] - '0');
    if (parsed > UINT32_MAX) return false;
  }
  return true;
}

bool hexValue(char value, uint8_t& output) {
  if (value >= '0' && value <= '9') output = static_cast<uint8_t>(value - '0');
  else if (value >= 'a' && value <= 'f') output = static_cast<uint8_t>(value - 'a' + 10);
  else if (value >= 'A' && value <= 'F') output = static_cast<uint8_t>(value - 'A' + 10);
  else return false;
  return true;
}

bool parseHex32(const char* value, size_t length, uint32_t& output) {
  if (length != 8) return false;
  uint32_t parsed = 0;
  for (size_t i = 0; i < length; ++i) {
    uint8_t digit = 0;
    if (!hexValue(value[i], digit)) return false;
    parsed = (parsed << 4) | digit;
  }
  output = parsed;
  return true;
}

bool isHex(const char* value, size_t length, bool lowercaseOnly = false) {
  for (size_t i = 0; i < length; ++i) {
    if ((value[i] >= '0' && value[i] <= '9') ||
        (value[i] >= 'a' && value[i] <= 'f')) continue;
    if (!lowercaseOnly && value[i] >= 'A' && value[i] <= 'F') continue;
    return false;
  }
  return true;
}

bool segmentBasename(const char* input, char (&output)[EventLog::kBasenameCapacity],
                     uint32_t& counter) {
  if (input == nullptr) return false;
  const char* leaf = strrchr(input, '/');
  leaf = leaf == nullptr ? input : leaf + 1;
  if (strlen(leaf) != EventLog::kBasenameCapacity - 1 || leaf[0] != 'E' ||
      leaf[9] != '_' || memcmp(leaf + 42, ".csv", 4) != 0 ||
      !isHex(leaf + 1, 8) || !isHex(leaf + 10, 32)) return false;
  if (!parseHex32(leaf + 1, 8, counter)) return false;
  memcpy(output, leaf, EventLog::kBasenameCapacity - 1);
  output[EventLog::kBasenameCapacity - 1] = '\0';
  return true;
}

void makeEventId(const uint8_t (&nonce)[16], uint32_t sequence, char (&output)[41]) {
  for (size_t i = 0; i < 16; ++i) {
    output[i * 2] = kHex[nonce[i] >> 4];
    output[i * 2 + 1] = kHex[nonce[i] & 0x0F];
  }
  snprintf(output + 32, 9, "%08lx", static_cast<unsigned long>(sequence));
}

bool formatTimestamp(uint32_t epochSeconds, char (&output)[21]) {
  output[0] = '\0';
  if (epochSeconds == 0) return true;
  const time_t raw = static_cast<time_t>(epochSeconds);
  struct tm parts = {};
#if defined(_MSC_VER)
  if (gmtime_s(&parts, &raw) != 0) return false;
#else
  if (gmtime_r(&raw, &parts) == nullptr) return false;
#endif
  if (parts.tm_year < 100 || parts.tm_year > 8099) return false;
  const int length = snprintf(output, sizeof(output), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                              parts.tm_year + 1900, parts.tm_mon + 1,
                              parts.tm_mday, parts.tm_hour, parts.tm_min,
                              parts.tm_sec);
  return length == 20;
}

struct Field {
  const char* data;
  size_t length;
};

bool validateRow(const char* row, size_t length, char (&id)[41], uint8_t schema) {
  Field fields[20] = {};
  size_t fieldCount = 0;
  size_t start = 0;
  bool quoted = false;
  for (size_t i = 0; i <= length; ++i) {
    if (i < length && row[i] == '"') {
      if (quoted && i + 1 < length && row[i + 1] == '"') { ++i; continue; }
      quoted = !quoted;
    }
    if ((i == length || row[i] == ',') && !quoted) {
      if (fieldCount >= 20) return false;
      fields[fieldCount++] = {row + start, i - start};
      start = i + 1;
    }
  }
  if (quoted) return false;
  const size_t expectedFields = schema == 0 ? 10U : schema == 1 ? 12U : 19U;
  const size_t crcIndex = expectedFields - 1;
  if (fieldCount != expectedFields || fields[0].length != 40 ||
      !isHex(fields[0].data, fields[0].length, true) ||
      !validTimestamp(fields[1].data, fields[1].length) ||
      !validIdField(fields[3].data, fields[3].length, 'U') ||
      !validIdField(fields[4].data, fields[4].length, 'D') ||
      !validActionField(fields[5].data, fields[5].length) ||
      !validResultField(fields[6].data, fields[6].length) ||
      !validUptime(fields[8].data, fields[8].length) ||
      fields[crcIndex].length != 8) return false;
  const bool uptimeQuality = fields[2].length == 6 &&
                             memcmp(fields[2].data, "UPTIME", 6) == 0;
  const bool ntpQuality = fields[2].length == 3 &&
                          memcmp(fields[2].data, "NTP", 3) == 0;
  if ((!uptimeQuality && !ntpQuality) ||
      (fields[1].length == 0) != uptimeQuality ||
      !((fields[7].length == 2 && memcmp(fields[7].data, "AP", 2) == 0) ||
        (fields[7].length == 6 && memcmp(fields[7].data, "AP_STA", 6) == 0) ||
        (fields[7].length == 3 && memcmp(fields[7].data, "STA", 3) == 0)))
    return false;
  if (schema >= 1 && (!validIdField(fields[9].data, fields[9].length, 'D') ||
                  !((fields[10].length == 7 && memcmp(fields[10].data, "UNKNOWN", 7) == 0) ||
                    (fields[10].length == 3 && memcmp(fields[10].data, "LAN", 3) == 0) ||
                    (fields[10].length == 11 && memcmp(fields[10].data, "RECOVERY_AP", 11) == 0) ||
                    (fields[10].length == 14 && memcmp(fields[10].data, "PHYSICAL_ADMIN", 14) == 0)))) return false;
  if (schema == 2) {
    if (fields[11].length != 1 || fields[11].data[0] != '3' ||
        !validCsvName(fields[12].data, fields[12].length) ||
        !validRoleField(fields[13].data, fields[13].length) ||
        !validContextField(fields[14].data, fields[14].length) ||
        !validIdField(fields[15].data, fields[15].length, 'D') ||
        !validIdField(fields[16].data, fields[16].length, 'D') ||
        fields[17].length != 9 || memcmp(fields[17].data, "google-v1", 9) != 0)
      return false;
  }
  uint32_t expected = 0;
  const Field& crcField = fields[crcIndex];
  const size_t crcComma = static_cast<size_t>(crcField.data - row) - 1;
  if (!parseHex32(crcField.data, crcField.length, expected) ||
      crc32(reinterpret_cast<const uint8_t*>(row), crcComma) != expected)
    return false;
  memcpy(id, fields[0].data, 40);
  id[40] = '\0';
  return true;
}

bool validateSegmentBytes(const char* bytes, size_t length, size_t& rowCount) {
  rowCount = 0;
  const size_t newHeaderLength = sizeof(kCsvHeader) - 1;
  const size_t oldHeaderLength = sizeof(kLegacyCsvHeader) - 1;
  const size_t currentHeaderLength = sizeof(kCurrentCsvHeader) - 1;
  const bool legacy = length >= oldHeaderLength && memcmp(bytes, kLegacyCsvHeader, oldHeaderLength) == 0;
  const bool current = length >= currentHeaderLength && memcmp(bytes, kCurrentCsvHeader, currentHeaderLength) == 0;
  const uint8_t schema = legacy ? 0 : current ? 1 : 2;
  const size_t headerLength = legacy ? oldHeaderLength : current ? currentHeaderLength : newHeaderLength;
  if (length <= headerLength || length > EventLog::kMaxSegmentBytes ||
      (!legacy && !current && memcmp(bytes, kCsvHeader, newHeaderLength) != 0) || bytes[length - 1] != '\n')
    return false;
  char seenIds[EventLog::kMaxRecordsPerSegment][41] = {};
  size_t cursor = headerLength;
  while (cursor < length) {
    const char* newline = static_cast<const char*>(memchr(bytes + cursor, '\n', length - cursor));
    if (newline == nullptr || newline == bytes + cursor ||
        rowCount >= EventLog::kMaxRecordsPerSegment) return false;
    const size_t lineLength = static_cast<size_t>(newline - (bytes + cursor));
    char id[41] = {};
    if (!validateRow(bytes + cursor, lineLength, id, schema)) return false;
    for (size_t i = 0; i < rowCount; ++i)
      if (memcmp(seenIds[i], id, sizeof(id)) == 0) return false;
    memcpy(seenIds[rowCount], id, sizeof(id));
    ++rowCount;
    cursor += lineLength + 1;
  }
  return cursor == length && rowCount > 0;
}

bool loadAndValidate(const char* basename, char* output, size_t capacity,
                     size_t& length) {
  length = 0;
  char canonical[EventLog::kBasenameCapacity] = {};
  uint32_t ignoredCounter = 0;
  if (!segmentBasename(basename, canonical, ignoredCounter) ||
      strcmp(canonical, basename) != 0) return false;
  char path[sizeof(kOutboxPath) + EventLog::kBasenameCapacity] = {};
  if (snprintf(path, sizeof(path), "%s/%s", kOutboxPath, canonical) >=
      static_cast<int>(sizeof(path))) return false;
  File file = SD.open(path, FILE_READ);
  if (!file) return false;
  const size_t size = file.size();
  if (size > EventLog::kMaxSegmentBytes || size == 0 || capacity <= size) {
    file.close();
    return false;
  }
  const size_t got = file.readBytes(output, size);
  file.close();
  if (got != size || !validateSegmentBytes(output, got, length)) {
    length = 0;
    return false;
  }
  output[got] = '\0';
  length = got;
  return true;
}

}  // namespace

EventLog::EventLog()
    : queueHead_(0), queueCount_(0), droppedEvents_(0), historyLostEvents_(0),
      historyLostSegments_(0), unrecordedEvents_(0), unrecordedObserved_(0),
      unrecordedSinceReservation_(0), nextSequence_(0),
      sequenceExhausted_(false), bootNonce_{}, enabled_(false),
      storageFault_(false), outboxFull_(false), pendingHistoryProtected_(false),
      protectedQueueFull_(false), lossPersistenceFault_(false), currentSegmentBlocked_(false),
      hasCorruptSegments_(false), hasUnrecognizedFiles_(false), segmentCount_(0), nextSegmentCounter_(0),
      segmentCounterExhausted_(false), currentSegmentOpen_(false),
      currentRecordCount_(0), currentSegmentCounter_(0), currentSegmentBytes_(0),
      currentSegmentFirstEventUptimeMs_(0), currentSegmentHasFirstEvent_(false),
      currentBasename_{}, selectedBasename_{}, pendingAckBasename_{}, selectedReadValidated_(false) {}

bool EventLog::begin(bool sdAvailable) {
  enabled_ = false;
  retainedEvents_=0;retainedCountKnown_=false;
  storageFault_ = false;
  outboxFull_ = false;
  protectedQueueFull_ = false;
  lossPersistenceFault_ = false;
  pendingHistoryProtected_ = false;
  currentSegmentBlocked_ = false;
  hasCorruptSegments_ = false;
  hasUnrecognizedFiles_ = false;
  segmentCount_ = 0;
  nextSegmentCounter_ = 0;
  segmentCounterExhausted_ = false;
  currentSegmentOpen_ = false;
  currentRecordCount_ = 0;
  currentSegmentBytes_ = 0;
  currentSegmentFirstEventUptimeMs_ = 0;
  currentSegmentHasFirstEvent_ = false;
  currentBasename_[0] = '\0';
  selectedBasename_[0] = '\0';
  pendingAckBasename_[0] = '\0';
  selectedReadValidated_ = false;
  nextSequence_ = 0;
  sequenceExhausted_ = false;
  unrecordedObserved_ = 0;
  Preferences history;
  if (!history.begin("sl-audit", false)) {
    storageFault_ = true;
    return false;
  }
  struct HistoryLoss { uint32_t events; uint32_t segments; uint32_t unrecorded; uint8_t protect; uint8_t reserved[3]; } loss = {};
  const size_t lossLength = history.getBytesLength("loss");
  if (lossLength == 8) {
    struct LegacyLoss { uint32_t events; uint32_t segments; } old = {};
    if (history.getBytes("loss", &old, sizeof(old)) != sizeof(old)) storageFault_ = true;
    else { loss.events = old.events; loss.segments = old.segments; }
  } else if (lossLength == sizeof(loss) && history.getBytes("loss", &loss, sizeof(loss)) == sizeof(loss)) {
    // Current record loaded atomically.
  } else if (lossLength != 0) {
    storageFault_ = true;
    // Unknown retention state must fail closed: do not resume destructive
    // rotation when durable protection metadata cannot be interpreted.
    pendingHistoryProtected_ = true;
  }
  if (lossLength == 8 || lossLength == sizeof(loss)) {
    historyLostEvents_ = loss.events;
    historyLostSegments_ = loss.segments;
    unrecordedEvents_ = loss.unrecorded;
    unrecordedSinceReservation_ = loss.unrecorded == 0 ? 0 : 64;
    pendingHistoryProtected_ = loss.protect == 1;
  } else if (lossLength == 0) {
    historyLostEvents_ = historyLostSegments_ = unrecordedEvents_ = 0;
    unrecordedSinceReservation_ = 0;
    pendingHistoryProtected_ = false;
  }
  history.end();

  wifi_mode_t wifiMode = WIFI_MODE_NULL;
  const bool radioActive = esp_wifi_get_mode(&wifiMode) == ESP_OK &&
                           wifiMode != WIFI_MODE_NULL;
  if (!radioActive) bootloader_random_enable();
  esp_fill_random(bootNonce_, sizeof(bootNonce_));
  if (!radioActive) bootloader_random_disable();
  bool nonceNonzero = false;
  for (uint8_t byte : bootNonce_) nonceNonzero |= byte != 0;
  if (!nonceNonzero) {
    storageFault_ = true;
    enabled_ = false;
    return false;
  }

  if (!sdAvailable) return false;
  if (!SD.exists(kOutboxPath)) {
    if (!SD.mkdir(kOutboxPath)) {
      storageFault_ = true;
      return false;
    }
  }
  File directory = SD.open(kOutboxPath, FILE_READ);
  if (!directory || !directory.isDirectory()) {
    if (directory) directory.close();
    storageFault_ = true;
    return false;
  }
  directory.close();
  enabled_ = inspectOutbox();
  if (!enabled_) storageFault_ = true;
  if(enabled_) {
    SegmentInfo* entries=static_cast<SegmentInfo*>(calloc(kMaxSegments,sizeof(SegmentInfo)));size_t count=0;
    if(entries&&enumerateSegments(entries,kMaxSegments,count)) {for(size_t i=0;i<count;++i)retainedEvents_+=entries[i].records;
      retainedCountKnown_=!hasCorruptSegments_&&!hasUnrecognizedFiles_;}
    free(entries);
  }
  return enabled_;
}

bool EventLog::inspectOutbox() {
  File directory = SD.open(kOutboxPath, FILE_READ);
  if (!directory || !directory.isDirectory()) {
    if (directory) directory.close();
    return false;
  }
  uint8_t segments = 0;
  bool hasCounter = false;
  uint32_t maxCounter = 0;
  while (true) {
    File entry = directory.openNextFile();
    if (!entry) break;
    char parsed[ kBasenameCapacity ] = {};
    uint32_t counter = 0;
    const bool recognized = !entry.isDirectory() &&
                            segmentBasename(entry.name(), parsed, counter);
    if (!recognized) hasUnrecognizedFiles_ = true;
    else {
      if (segments < UINT8_MAX) ++segments;
      if (!hasCounter || counter > maxCounter) {
        hasCounter = true;
        maxCounter = counter;
      }
    }
    entry.close();
  }
  directory.close();
  segmentCount_ = segments > kMaxSegments ? kMaxSegments : segments;
  outboxFull_ = segments >= kMaxSegments;
  protectedQueueFull_ = pendingHistoryProtected_ && outboxFull_;
  if (hasCounter) {
    if (maxCounter == UINT32_MAX) segmentCounterExhausted_ = true;
    else nextSegmentCounter_ = maxCounter + 1;
  }
  if (hasUnrecognizedFiles_) storageFault_ = true;
  return true;
}

bool EventLog::enqueue(Action action, Result result, const char* userId,
                       const char* deviceId, uint32_t epochSeconds,
                       uint32_t uptimeMs, NetworkMode networkMode,
                       Source source, const char* claimedDeviceId,
                       const char* identityName, const char* role,
                       const char* identityContext, const char* actorIdentityId,
                       const char* subjectIdentityId) {
  if (!enabled_ || sequenceExhausted_ || queueCount_ >= kQueueCapacity ||
      actionName(action) == nullptr || resultName(result) == nullptr ||
      (networkMode != NetworkMode::AP && networkMode != NetworkMode::AP_STA && networkMode != NetworkMode::STA) ||
      !validId(userId, 'U') || !validId(deviceId, 'D') ||
      !validId(claimedDeviceId, 'D') || sourceName(source) == nullptr ||
      !validName(identityName) || !validRole(role) || !validContext(identityContext) ||
      !validId(actorIdentityId, 'D') || !validId(subjectIdentityId, 'D')) {
    noteDrop();
    return false;
  }
  Event& event = queue_[(queueHead_ + queueCount_) % kQueueCapacity];
  event.sequence = nextSequence_;
  if (nextSequence_ == UINT32_MAX) sequenceExhausted_ = true;
  else ++nextSequence_;
  event.epochSeconds = epochSeconds;
  event.uptimeMs = uptimeMs;
  event.action = action;
  event.result = result;
  event.networkMode = networkMode;
  event.source = source;
  copyId(event.userId, userId);
  copyId(event.deviceId, deviceId);
  copyId(event.claimedDeviceId, claimedDeviceId);
  snprintf(event.identityName, sizeof(event.identityName), "%s", identityName);
  snprintf(event.role, sizeof(event.role), "%s", role);
  snprintf(event.identityContext, sizeof(event.identityContext), "%s", identityContext);
  copyId(event.actorIdentityId, actorIdentityId);
  copyId(event.subjectIdentityId, subjectIdentityId);
  ++queueCount_;
  return true;
}

bool EventLog::protectPendingHistory() {
  if (pendingHistoryProtected_) return true;
  Preferences history;
  if (!history.begin("sl-audit", false)) { storageFault_ = lossPersistenceFault_ = true; return false; }
  struct HistoryLoss { uint32_t events; uint32_t segments; uint32_t unrecorded; uint8_t protect; uint8_t reserved[3]; } loss = {};
  const size_t size = history.getBytesLength("loss");
  if (size == 8) {
    struct LegacyLoss { uint32_t events; uint32_t segments; } old = {};
    if (history.getBytes("loss", &old, sizeof(old)) != sizeof(old)) { history.end(); storageFault_ = lossPersistenceFault_ = true; return false; }
    loss.events = old.events; loss.segments = old.segments;
  } else if (size == sizeof(loss)) {
    if (history.getBytes("loss", &loss, sizeof(loss)) != sizeof(loss)) { history.end(); storageFault_ = lossPersistenceFault_ = true; return false; }
  } else if (size != 0) { history.end(); storageFault_ = lossPersistenceFault_ = true; return false; }
  loss.protect = 1;
  const bool saved = history.putBytes("loss", &loss, sizeof(loss)) == sizeof(loss);
  history.end();
  if (!saved) { storageFault_ = lossPersistenceFault_ = true; return false; }
  pendingHistoryProtected_ = true;
  unrecordedEvents_ = loss.unrecorded;
  return true;
}

bool EventLog::protectedHistory() const { return pendingHistoryProtected_; }

void EventLog::update() {
  if (queueCount_ == 0) return;
  if (!enabled_) {
    dropFront();
    noteDrop();
    return;
  }
  if (!currentSegmentOpen_) {
    if (!createCurrentSegment()) {
      dropFront();
      noteDrop();
    }
    return;
  }
  appendQueuedEvent();
}

bool EventLog::createCurrentSegment() {
  if (segmentCount_ >= kMaxSegments &&
      (pendingHistoryProtected_ || !evictOldestClosedSegment())) {
    outboxFull_ = true;
    protectedQueueFull_ = pendingHistoryProtected_;
    storageFault_ = true;
    return false;
  }
  if (segmentCounterExhausted_) {
    outboxFull_ = true;
    storageFault_ = true;
    return false;
  }
  char path[sizeof(kOutboxPath) + kBasenameCapacity] = {};
  char candidate[kBasenameCapacity] = {};
  uint32_t counter = nextSegmentCounter_;
  bool madeName = false;
  for (uint8_t attempt = 0; attempt < kMaxSegments; ++attempt) {
    if (snprintf(candidate, sizeof(candidate), "E%08lX_", static_cast<unsigned long>(counter)) != 10) {
      storageFault_ = true;
      return false;
    }
    size_t offset = 10;
    for (uint8_t byte : bootNonce_) {
      candidate[offset++] = "0123456789ABCDEF"[byte >> 4];
      candidate[offset++] = "0123456789ABCDEF"[byte & 0x0F];
    }
    memcpy(candidate + offset, ".csv", 5);
    if (snprintf(path, sizeof(path), "%s/%s", kOutboxPath, candidate) >=
        static_cast<int>(sizeof(path))) return false;
    if (!SD.exists(path)) {
      madeName = true;
      break;
    }
    if (counter == UINT32_MAX) break;
    ++counter;
  }
  if (!madeName) {
    storageFault_ = true;
    outboxFull_ = segmentCount_ >= kMaxSegments;
    return false;
  }
  if (counter == UINT32_MAX) segmentCounterExhausted_ = true;
  else nextSegmentCounter_ = counter + 1;

  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    storageFault_ = true;
    if (SD.exists(path)) {
      if (segmentCount_ < kMaxSegments) ++segmentCount_;
      hasUnrecognizedFiles_ = true;
      currentSegmentBlocked_ = true;
    }
    return false;
  }
  const size_t headerLength = sizeof(kCsvHeader) - 1;
  const bool written = file.write(reinterpret_cast<const uint8_t*>(kCsvHeader),
                                  headerLength) == headerLength;
  file.flush();
  const size_t finalSize = file.size();
  file.close();
  if (segmentCount_ < kMaxSegments) ++segmentCount_;
  outboxFull_ = segmentCount_ >= kMaxSegments;
  if (!written || finalSize != headerLength) {
    storageFault_ = true;
    currentSegmentBlocked_ = true;
    hasUnrecognizedFiles_ = true;
    return false;
  }
  memcpy(currentBasename_, candidate, sizeof(currentBasename_));
  currentSegmentOpen_ = true;
  currentRecordCount_ = 0;
  currentSegmentCounter_ = counter;
  currentSegmentBytes_ = headerLength;
  currentSegmentFirstEventUptimeMs_ = 0;
  currentSegmentHasFirstEvent_ = false;
  return true;
}

bool EventLog::appendQueuedEvent() {
  if (queueCount_ == 0 || !currentSegmentOpen_) return false;
  Event& event = queue_[queueHead_];
  char timestamp[21] = {};
  if (!formatTimestamp(event.epochSeconds, timestamp)) {
    storageFault_ = true;
    dropFront();
    noteDrop();
    return false;
  }
  char eventId[41] = {};
  makeEventId(bootNonce_, event.sequence, eventId);
  const char* quality = event.epochSeconds == 0 ? "UPTIME" : "NTP";
  const char* mode = event.networkMode == NetworkMode::STA ? "STA" : event.networkMode == NetworkMode::AP_STA ? "AP_STA" : "AP";
  char row[kMaxRowBytes] = {};
  char encodedName[83] = {};
  size_t nameOffset = 0;
  encodedName[nameOffset++] = '"';
  for (const char* p = event.identityName; *p && nameOffset + 2 < sizeof(encodedName); ++p) {
    encodedName[nameOffset++] = *p;
    if (*p == '"') encodedName[nameOffset++] = '"';
  }
  encodedName[nameOffset++] = '"';
  encodedName[nameOffset] = '\0';
  const int rowLength = snprintf(row, sizeof(row),
                                "%s,%s,%s,%s,%s,%s,%s,%s,%lu,%s,%s,3,%s,%s,%s,%s,%s,google-v1",
                                eventId, timestamp, quality, event.userId,
                                event.deviceId, actionName(event.action),
                                resultName(event.result), mode,
                                static_cast<unsigned long>(event.uptimeMs),
                                event.claimedDeviceId, sourceName(event.source),
                                encodedName, event.role, event.identityContext,
                                event.actorIdentityId, event.subjectIdentityId);
  if (rowLength <= 0 || static_cast<size_t>(rowLength) >= sizeof(row)) {
    storageFault_ = true;
    dropFront();
    noteDrop();
    return false;
  }
  const uint32_t checksum = crc32(reinterpret_cast<const uint8_t*>(row),
                                  static_cast<size_t>(rowLength));
  char completeRow[kMaxRowBytes] = {};
  const int completeLength = snprintf(completeRow, sizeof(completeRow), "%s,%08lX\n",
                                      row, static_cast<unsigned long>(checksum));
  if (completeLength <= 0 || static_cast<size_t>(completeLength) >= sizeof(completeRow) ||
      currentRecordCount_ >= kMaxRecordsPerSegment ||
      currentSegmentBytes_ + static_cast<size_t>(completeLength) > kMaxSegmentBytes) {
    currentSegmentOpen_ = false;
    currentBasename_[0] = '\0';
    return false;
  }

  char path[sizeof(kOutboxPath) + kBasenameCapacity] = {};
  if (snprintf(path, sizeof(path), "%s/%s", kOutboxPath, currentBasename_) >=
      static_cast<int>(sizeof(path))) {
    currentSegmentOpen_ = false;
    currentSegmentBlocked_ = true;
    storageFault_ = true;
    dropFront();
    noteDrop();
    return false;
  }
  File file = SD.open(path, FILE_APPEND);
  if (!file || file.size() != currentSegmentBytes_) {
    if (file) file.close();
    currentSegmentOpen_ = false;
    currentSegmentBlocked_ = true;
    storageFault_ = true;
    dropFront();
    noteDrop();
    return false;
  }
  const bool written = file.write(reinterpret_cast<const uint8_t*>(completeRow),
                                  static_cast<size_t>(completeLength)) ==
                       static_cast<size_t>(completeLength);
  file.flush();
  const size_t finalSize = file.size();
  file.close();
  if (!written || finalSize != currentSegmentBytes_ + static_cast<size_t>(completeLength)) {
    currentSegmentOpen_ = false;
    currentSegmentBlocked_ = true;
    storageFault_ = true;
    dropFront();
    noteDrop();
    return false;
  }
  currentSegmentBytes_ = finalSize;
  if (!currentSegmentHasFirstEvent_) {
    currentSegmentFirstEventUptimeMs_ = event.uptimeMs;
    currentSegmentHasFirstEvent_ = true;
  }
  ++currentRecordCount_;
  ++retainedEvents_;
  dropFront();
  if (currentRecordCount_ >= kMaxRecordsPerSegment) {
    currentSegmentOpen_ = false;
    currentBasename_[0] = '\0';
  }
  return true;
}

void EventLog::dropFront() {
  if (queueCount_ == 0) return;
  memset(&queue_[queueHead_], 0, sizeof(queue_[queueHead_]));
  queueHead_ = static_cast<uint8_t>((queueHead_ + 1) % kQueueCapacity);
  --queueCount_;
}

void EventLog::noteDrop() {
  if (droppedEvents_ != UINT32_MAX) ++droppedEvents_;
  if (pendingHistoryProtected_ && outboxFull_) protectedQueueFull_ = true;
  persistUnrecordedEvent();
}

void EventLog::status(Status& output) const {
  output.enabled = enabled_;
  output.storageFault = storageFault_;
  output.outboxFull = outboxFull_;
  output.currentSegmentBlocked = currentSegmentBlocked_;
  output.hasCorruptSegments = hasCorruptSegments_;
  output.hasUnrecognizedFiles = hasUnrecognizedFiles_;
  output.sequenceExhausted = sequenceExhausted_;
  output.queueDepth = queueCount_;
  output.segmentCount = segmentCount_;
  output.droppedEvents = droppedEvents_;
  output.retainedEvents=retainedEvents_;
  output.retainedCountKnown=retainedCountKnown_&&!hasCorruptSegments_&&!hasUnrecognizedFiles_;
  output.historyLostEvents = historyLostEvents_;
  output.historyLostSegments = historyLostSegments_;
  output.pendingHistoryProtected = pendingHistoryProtected_;
  output.protectedQueueFull = protectedQueueFull_;
  output.lossPersistenceFault = lossPersistenceFault_;
  output.unrecordedEvents = unrecordedObserved_ > unrecordedEvents_
      ? unrecordedObserved_ : unrecordedEvents_;
  output.currentSegmentHasEvents = currentSegmentHasFirstEvent_;
  output.currentSegmentFirstEventUptimeMs = currentSegmentFirstEventUptimeMs_;
}

bool EventLog::persistUnrecordedEvent() {
  constexpr uint32_t kReservation = 64;
  if (unrecordedObserved_ != UINT32_MAX) ++unrecordedObserved_;
  if (lossPersistenceFault_) return false;
  if (unrecordedSinceReservation_ < kReservation && unrecordedEvents_ != 0) {
    ++unrecordedSinceReservation_;
    return true;
  }
  const uint32_t next = UINT32_MAX - unrecordedEvents_ < kReservation
      ? UINT32_MAX : unrecordedEvents_ + kReservation;
  if (next == unrecordedEvents_) return true;
  Preferences history;
  if (!history.begin("sl-audit", false)) { storageFault_ = lossPersistenceFault_ = true; return false; }
  struct HistoryLoss { uint32_t events; uint32_t segments; uint32_t unrecorded; uint8_t protect; uint8_t reserved[3]; } loss = {};
  const size_t size = history.getBytesLength("loss");
  if (size == 8) {
    struct LegacyLoss { uint32_t events; uint32_t segments; } old = {};
    if (history.getBytes("loss", &old, sizeof(old)) != sizeof(old)) { history.end(); storageFault_ = lossPersistenceFault_ = true; return false; }
    loss.events = old.events; loss.segments = old.segments;
  } else if (size == sizeof(loss)) {
    if (history.getBytes("loss", &loss, sizeof(loss)) != sizeof(loss)) { history.end(); storageFault_ = lossPersistenceFault_ = true; return false; }
  } else if (size != 0) { history.end(); storageFault_ = lossPersistenceFault_ = true; return false; }
  loss.unrecorded = next;
  if (pendingHistoryProtected_) loss.protect = 1;
  const bool saved = history.putBytes("loss", &loss, sizeof(loss)) == sizeof(loss);
  history.end();
  if (!saved) { storageFault_ = lossPersistenceFault_ = true; return false; }
  unrecordedEvents_ = next;
  unrecordedSinceReservation_ = 1;
  return true;
}

bool EventLog::sealCurrentSegment() {
  if (queueCount_ != 0 || !currentSegmentOpen_ || currentRecordCount_ == 0)
    return false;
  currentSegmentOpen_ = false;
  currentBasename_[0] = '\0';
  return true;
}

bool EventLog::selectOldestClosedSegment(char* basename, size_t capacity) {
  if (basename == nullptr || capacity < kBasenameCapacity || !enabled_) return false;
  selectedBasename_[0] = '\0';
  selectedReadValidated_ = false;
  struct Candidate {
    char name[kBasenameCapacity];
    uint32_t counter;
  };
  Candidate* candidates = static_cast<Candidate*>(calloc(kMaxSegments, sizeof(Candidate)));
  char* validationBuffer = static_cast<char*>(malloc(kSegmentBufferCapacity));
  if (candidates == nullptr || validationBuffer == nullptr) {
    free(candidates);
    free(validationBuffer);
    storageFault_ = true;
    return false;
  }
  size_t count = 0;
  bool scanOverflow = false;
  File directory = SD.open(kOutboxPath, FILE_READ);
  if (!directory || !directory.isDirectory()) {
    if (directory) directory.close();
    storageFault_ = true;
    free(candidates);
    free(validationBuffer);
    return false;
  }
  while (true) {
    File entry = directory.openNextFile();
    if (!entry) break;
    char parsed[kBasenameCapacity] = {};
    uint32_t counter = 0;
    const bool recognized = !entry.isDirectory() &&
                            segmentBasename(entry.name(), parsed, counter);
    if (!recognized) hasUnrecognizedFiles_ = true;
    else if (!currentSegmentOpen_ || strcmp(parsed, currentBasename_) != 0) {
      if (count >= kMaxSegments) scanOverflow = true;
      else {
        memcpy(candidates[count].name, parsed, sizeof(parsed));
        candidates[count].counter = counter;
        ++count;
      }
    }
    entry.close();
  }
  directory.close();
  if (scanOverflow) {
    outboxFull_ = true;
    storageFault_ = true;
    free(candidates);
    free(validationBuffer);
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
    size_t minIndex = i;
    for (size_t j = i + 1; j < count; ++j) {
      if (candidates[j].counter < candidates[minIndex].counter ||
          (candidates[j].counter == candidates[minIndex].counter &&
           strcmp(candidates[j].name, candidates[minIndex].name) < 0))
        minIndex = j;
    }
    if (minIndex != i) {
      const Candidate swap = candidates[i];
      candidates[i] = candidates[minIndex];
      candidates[minIndex] = swap;
    }
  }
  for (size_t i = 0; i < count; ++i) {
    size_t length = 0;
    if (!loadAndValidate(candidates[i].name, validationBuffer,
                         kSegmentBufferCapacity, length)) {
      storageFault_ = true;
      hasCorruptSegments_ = true;
      continue;
    }
    memcpy(selectedBasename_, candidates[i].name, sizeof(selectedBasename_));
    selectedReadValidated_ = false;
    memcpy(basename, candidates[i].name, sizeof(selectedBasename_));
    free(candidates);
    free(validationBuffer);
    return true;
  }
  free(candidates);
  free(validationBuffer);
  return false;
}

bool EventLog::readClosedSegment(const char* basename, char* output,
                                 size_t capacity, size_t& outputLength) {
  outputLength = 0;
  if (basename == nullptr || output == nullptr || selectedBasename_[0] == '\0' ||
      strcmp(basename, selectedBasename_) != 0 ||
      (currentSegmentOpen_ && strcmp(basename, currentBasename_) == 0)) return false;
  if (!loadAndValidate(basename, output, capacity, outputLength)) {
    storageFault_ = true;
    hasCorruptSegments_ = true;
    selectedReadValidated_ = false;
    outputLength = 0;
    return false;
  }
  selectedReadValidated_ = true;
  return true;
}

bool EventLog::deleteAckedSegment(const char* basename) {
  if (basename == nullptr || !selectedReadValidated_ ||
      selectedBasename_[0] == '\0' || strcmp(basename, selectedBasename_) != 0 ||
      (currentSegmentOpen_ && strcmp(basename, currentBasename_) == 0)) return false;
  char* validationBuffer = static_cast<char*>(malloc(kSegmentBufferCapacity));
  if (validationBuffer == nullptr) {
    storageFault_ = true;
    return false;
  }
  size_t length = 0;
  if (!loadAndValidate(basename, validationBuffer, kSegmentBufferCapacity, length)) {
    free(validationBuffer);
    storageFault_ = true;
    hasCorruptSegments_ = true;
    selectedReadValidated_ = false;
    return false;
  }
  size_t retiredRecords=0;validateSegmentBytes(validationBuffer,length,retiredRecords);
  free(validationBuffer);
  char path[sizeof(kOutboxPath) + kBasenameCapacity] = {};
  if (snprintf(path, sizeof(path), "%s/%s", kOutboxPath, selectedBasename_) >=
      static_cast<int>(sizeof(path)) || !SD.remove(path)) {
    storageFault_ = true;
    return false;
  }
  retainedEvents_=retainedEvents_>=retiredRecords?retainedEvents_-static_cast<uint32_t>(retiredRecords):0;
  selectedBasename_[0] = '\0';
  selectedReadValidated_ = false;
  if (pendingAckBasename_[0] != '\0' && strcmp(pendingAckBasename_, basename) == 0)
    pendingAckBasename_[0] = '\0';
  if (!inspectOutbox()) storageFault_ = true;
  return true;
}

bool EventLog::persistHistoryLoss(uint32_t events, uint32_t segments) {
  const uint32_t nextEvents = UINT32_MAX - historyLostEvents_ < events
                                  ? UINT32_MAX : historyLostEvents_ + events;
  const uint32_t nextSegments = UINT32_MAX - historyLostSegments_ < segments
                                    ? UINT32_MAX : historyLostSegments_ + segments;
  if (nextEvents == historyLostEvents_ && nextSegments == historyLostSegments_) return true;
  Preferences history;
  if (!history.begin("sl-audit", false)) { storageFault_ = lossPersistenceFault_ = true; return false; }
  struct HistoryLoss { uint32_t events; uint32_t segments; uint32_t unrecorded; uint8_t protect; uint8_t reserved[3]; } loss = {};
  const size_t size = history.getBytesLength("loss");
  if (size == 8) {
    struct LegacyLoss { uint32_t events; uint32_t segments; } old = {};
    if (history.getBytes("loss", &old, sizeof(old)) != sizeof(old)) { history.end(); storageFault_ = lossPersistenceFault_ = true; return false; }
    loss.events = old.events; loss.segments = old.segments;
  } else if (size == sizeof(loss)) {
    if (history.getBytes("loss", &loss, sizeof(loss)) != sizeof(loss)) { history.end(); storageFault_ = lossPersistenceFault_ = true; return false; }
  } else if (size != 0) { history.end(); storageFault_ = lossPersistenceFault_ = true; return false; }
  loss.events = nextEvents;
  loss.segments = nextSegments;
  if (pendingHistoryProtected_) loss.protect = 1;
  const bool saved = history.putBytes("loss", &loss, sizeof(loss)) == sizeof(loss);
  history.end();
  if (!saved) { storageFault_ = lossPersistenceFault_ = true; return false; }
  historyLostEvents_ = nextEvents;
  historyLostSegments_ = nextSegments;
  return true;
}

bool EventLog::evictOldestClosedSegment() {
  struct Candidate { char name[kBasenameCapacity]; uint32_t counter; };
  Candidate* candidates = static_cast<Candidate*>(calloc(kMaxSegments, sizeof(Candidate)));
  char* validationBuffer = static_cast<char*>(malloc(kSegmentBufferCapacity));
  if (!candidates || !validationBuffer) {
    free(candidates); free(validationBuffer); storageFault_ = true; return false;
  }
  size_t count = 0;
  File directory = SD.open(kOutboxPath, FILE_READ);
  if (!directory || !directory.isDirectory()) {
    if (directory) directory.close();
    free(candidates); free(validationBuffer); storageFault_ = true; return false;
  }
  while (true) {
    File entry = directory.openNextFile();
    if (!entry) break;
    char parsed[kBasenameCapacity] = {};
    uint32_t counter = 0;
    if (!entry.isDirectory() && segmentBasename(entry.name(), parsed, counter) &&
        (!currentSegmentOpen_ || strcmp(parsed, currentBasename_) != 0) &&
        (pendingAckBasename_[0] == '\0' || strcmp(parsed, pendingAckBasename_) != 0) && count < kMaxSegments) {
      memcpy(candidates[count].name, parsed, sizeof(parsed));
      candidates[count++].counter = counter;
    }
    entry.close();
  }
  directory.close();
  for (size_t i = 0; i < count; ++i) {
    size_t minIndex = i;
    for (size_t j = i + 1; j < count; ++j)
      if (candidates[j].counter < candidates[minIndex].counter ||
          (candidates[j].counter == candidates[minIndex].counter &&
           strcmp(candidates[j].name, candidates[minIndex].name) < 0)) minIndex = j;
    if (minIndex != i) { Candidate swap = candidates[i]; candidates[i] = candidates[minIndex]; candidates[minIndex] = swap; }
  }
  bool removed = false;
  for (size_t i = 0; i < count; ++i) {
    size_t length = 0, records = 0;
    if (!loadAndValidate(candidates[i].name, validationBuffer, kSegmentBufferCapacity, length)) {
      hasCorruptSegments_ = true; storageFault_ = true; continue;
    }
    if (!validateSegmentBytes(validationBuffer, length, records)) continue;
    // Persist loss evidence before removing any historical bytes. A failed NVS
    // write leaves the segment untouched; a failed remove can conservatively
    // overcount but can never hide that deletion was attempted.
    if (!persistHistoryLoss(static_cast<uint32_t>(records), 1)) break;
    char path[sizeof(kOutboxPath) + kBasenameCapacity] = {};
    if (snprintf(path, sizeof(path), "%s/%s", kOutboxPath, candidates[i].name) >= static_cast<int>(sizeof(path)) ||
        !SD.remove(path)) { storageFault_ = true; break; }
    if (segmentCount_ > 0) --segmentCount_;
    retainedEvents_=retainedEvents_>=records?retainedEvents_-static_cast<uint32_t>(records):0;
    outboxFull_ = false;
    removed = true;
    break;
  }
  free(candidates); free(validationBuffer);
  return removed;
}

bool EventLog::enumerateSegments(SegmentInfo* output, size_t capacity, size_t& outputCount) {
  outputCount = 0;
  if (!enabled_ || (output == nullptr && capacity != 0)) return false;
  struct Candidate { char name[kBasenameCapacity]; uint32_t counter; };
  Candidate* candidates = static_cast<Candidate*>(calloc(kMaxSegments, sizeof(Candidate)));
  if (!candidates) { storageFault_ = true; return false; }
  size_t count = 0;
  bool overflow = false;
  File directory = SD.open(kOutboxPath, FILE_READ);
  if (!directory || !directory.isDirectory()) { if (directory) directory.close(); free(candidates); storageFault_ = true; return false; }
  while (true) {
    File entry = directory.openNextFile(); if (!entry) break;
    char parsed[kBasenameCapacity] = {}; uint32_t counter = 0;
    const bool parsedOk = !entry.isDirectory() && segmentBasename(entry.name(), parsed, counter);
    const bool emptyCurrent = parsedOk && currentSegmentOpen_ && currentRecordCount_ == 0 &&
                              strcmp(parsed, currentBasename_) == 0;
    if (parsedOk && !emptyCurrent && count < kMaxSegments) {
      memcpy(candidates[count].name, parsed, sizeof(parsed)); candidates[count++].counter = counter;
    } else if (parsedOk && !emptyCurrent) {
      overflow = true;
    } else if (!parsedOk && !entry.isDirectory()) hasUnrecognizedFiles_ = true;
    entry.close();
  }
  directory.close();
  if (overflow) { free(candidates); storageFault_ = true; return false; }
  for (size_t i=0;i<count;++i) for(size_t j=i+1;j<count;++j)
    if(candidates[j].counter<candidates[i].counter || (candidates[j].counter==candidates[i].counter && strcmp(candidates[j].name,candidates[i].name)<0)) {
      Candidate swap=candidates[i];candidates[i]=candidates[j];candidates[j]=swap;
    }
  char* buffer = static_cast<char*>(malloc(kSegmentBufferCapacity));
  if (!buffer) { free(candidates); storageFault_ = true; return false; }
  size_t returned = 0;
  for (size_t i=0;i<count && returned<capacity;++i) {
    size_t length=0, records=0;
    if (!loadAndValidate(candidates[i].name,buffer,kSegmentBufferCapacity,length) || !validateSegmentBytes(buffer,length,records)) {
      storageFault_=true;hasCorruptSegments_=true;continue;
    }
    memcpy(output[returned].id,candidates[i].name,kBasenameCapacity);
    output[returned].records=static_cast<uint8_t>(records);output[returned].counter=candidates[i].counter;
    ++returned;
  }
  free(buffer);
  free(candidates);
  outputCount = returned;
  return true;
}

bool EventLog::readSegmentById(const char* id, char* output, size_t capacity,
                               size_t& outputLength) {
  if (!enabled_ || !id || !output) { outputLength=0; return false; }
  return loadAndValidate(id,output,capacity,outputLength);
}

bool EventLog::pinPendingAckSegment(const char* id, bool pin) {
  if (!id) return false;
  if (!pin) {
    if (pendingAckBasename_[0] == '\0' || strcmp(id, pendingAckBasename_) != 0) return false;
    pendingAckBasename_[0] = '\0';
    return true;
  }
  char canonical[kBasenameCapacity] = {}; uint32_t counter = 0;
  if (pendingAckBasename_[0] != '\0' || !segmentBasename(id, canonical, counter) ||
      strcmp(id, canonical) != 0) return false;
  char* buffer = static_cast<char*>(malloc(kSegmentBufferCapacity));
  if (!buffer) return false;
  size_t length = 0;
  const bool valid = loadAndValidate(id, buffer, kSegmentBufferCapacity, length);
  free(buffer);
  if (!valid) return false;
  memcpy(pendingAckBasename_, canonical, sizeof(pendingAckBasename_));
  return true;
}

}  // namespace events
}  // namespace smartlock
