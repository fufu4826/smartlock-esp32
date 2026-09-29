#include "EventLog.h"
#include "SD.h"
#include "Preferences.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

SDClass SD;

namespace {
uint8_t randomByte = 0;
uint32_t testCrc32(const std::string& bytes) {
  uint32_t crc = 0xFFFFFFFFu;
  for (unsigned char byte : bytes) {
    crc ^= byte;
    for (uint8_t bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}
}

void esp_fill_random(void* output, size_t length) {
  uint8_t* bytes = static_cast<uint8_t*>(output);
  for (size_t i = 0; i < length; ++i) bytes[i] = randomByte++;
}

using smartlock::events::Action;
using smartlock::events::EventLog;
using smartlock::events::NetworkMode;
using smartlock::events::Result;

int main() {
  SD.clear();
  Preferences::clearAll();
  EventLog log;
  assert(log.begin(true));
  assert(log.enqueue(Action::AccessGranted, Result::Success, "U000042",
                     "D000007", 1780000000, 123456, NetworkMode::AP_STA));

  EventLog::Status status = {};
  log.status(status);
  assert(status.queueDepth == 1);
  assert(SD.fileCount() == 0);  // enqueue never touches SD

  log.update();                 // Creates one fresh segment/header.
  assert(SD.fileCount() == 1);
  log.status(status);
  assert(status.queueDepth == 1);
  log.update();                 // Appends one complete CRC-protected record.
  log.status(status);
  assert(status.queueDepth == 0);
  char basename[EventLog::kBasenameCapacity] = {};
  assert(!log.selectOldestClosedSegment(basename, sizeof(basename)));
  assert(log.sealCurrentSegment());

  assert(log.selectOldestClosedSegment(basename, sizeof(basename)));
  assert(strlen(basename) == 46);
  assert(!log.deleteAckedSegment(basename)); // Selection alone is not enough.

  char payload[EventLog::kSegmentBufferCapacity] = {};
  size_t payloadLength = 0;
  assert(log.readClosedSegment(basename, payload, sizeof(payload), payloadLength));
  assert(payloadLength > 0 && payload[payloadLength] == '\0');
  const std::string csv(payload, payloadLength);
  assert(csv.find("event_id,timestamp,time_quality,user_id,device_id,action,result,network_mode,uptime_ms,claimed_device_id,access_source,event_schema,identity_name,role,identity_context,actor_identity_id,subject_identity_id,firmware_version,crc32\n") == 0);
  assert(csv.find(",NTP,U000042,D000007,ACCESS_GRANTED,SUCCESS,AP_STA,123456,,UNKNOWN,3,\"\",,UNKNOWN,,,google-v1,") != std::string::npos);
  const std::string firstPayload = csv;
  assert(!log.deleteAckedSegment("E00000000_00000000000000000000000000000000.csv"));

  EventLog rebooted;
  assert(rebooted.begin(true));
  char afterReboot[EventLog::kBasenameCapacity] = {};
  assert(rebooted.selectOldestClosedSegment(afterReboot, sizeof(afterReboot)));
  assert(std::strcmp(afterReboot, basename) == 0);
  char persisted[EventLog::kSegmentBufferCapacity] = {};
  size_t persistedLength = 0;
  assert(rebooted.readClosedSegment(afterReboot, persisted, sizeof(persisted), persistedLength));
  assert(std::string(persisted, persistedLength) == firstPayload);
  assert(rebooted.enqueue(Action::Boot, Result::Success, "", "", 0, 50,
                          NetworkMode::AP));
  rebooted.update();
  rebooted.update();
  assert(SD.fileCount() == 2); // Reboot starts a new, monotonically numbered segment.
  assert(log.deleteAckedSegment(basename));
  assert(SD.fileCount() == 1);
  assert(!log.enqueue(Action::Unlock, Result::Success, "U000042x", "",
                      0, 1, NetworkMode::AP));
  log.status(status);
  assert(status.droppedEvents == 1);

  SD.clear();
  EventLog partial;
  assert(partial.begin(true));
  assert(partial.enqueue(Action::Unlock, Result::Success, "", "", 0, 3,
                         NetworkMode::AP));
  partial.update();
  SD.failNextWriteAfter(5);
  partial.update();
  partial.status(status);
  assert(status.droppedEvents == 1 && status.currentSegmentBlocked);
  assert(SD.fileCount() == 1); // Partial append is retained for diagnosis.
  char partialName[EventLog::kBasenameCapacity] = {};
  assert(!partial.selectOldestClosedSegment(partialName, sizeof(partialName)));
  assert(SD.fileCount() == 1);

  SD.clear();
  EventLog badCrc;
  assert(badCrc.begin(true));
  assert(badCrc.enqueue(Action::Relock, Result::Success, "", "", 0, 4,
                        NetworkMode::AP));
  badCrc.update();
  badCrc.update();
  assert(badCrc.sealCurrentSegment());
  const std::string crcPath = SD.files().begin()->first;
  std::string damagedCrc = SD.files().begin()->second;
  damagedCrc[damagedCrc.size() - 2] = damagedCrc[damagedCrc.size() - 2] == '0' ? '1' : '0';
  SD.setFileContents(crcPath, damagedCrc);
  EventLog::Status corruptionStatus = {};
  char corruptName[EventLog::kBasenameCapacity] = {};
  assert(!badCrc.selectOldestClosedSegment(corruptName, sizeof(corruptName)));
  badCrc.status(corruptionStatus);
  assert(corruptionStatus.hasCorruptSegments && corruptionStatus.storageFault);
  assert(SD.fileCount() == 1); // Corrupt, unsynced data is never deleted.
  assert(badCrc.enqueue(Action::Boot, Result::Success, "", "", 0, 5, NetworkMode::AP));
  badCrc.update(); badCrc.update(); assert(badCrc.sealCurrentSegment());
  EventLog::SegmentInfo afterCorruption[EventLog::kMaxSegments] = {};
  size_t afterCorruptionCount = 0;
  assert(badCrc.enumerateSegments(afterCorruption, EventLog::kMaxSegments, afterCorruptionCount));
  assert(afterCorruptionCount == 1 && badCrc.readSegmentById(afterCorruption[0].id, payload, sizeof(payload), payloadLength));
  assert(SD.fileCount() == 2); // Valid later history stays discoverable; corrupt bytes remain untouched.

  SD.clear();
  EventLog duplicate;
  assert(duplicate.begin(true));
  assert(duplicate.enqueue(Action::Boot, Result::Success, "", "", 0, 7,
                           NetworkMode::AP));
  duplicate.update();
  duplicate.update();
  assert(duplicate.sealCurrentSegment());
  const std::string duplicatePath = SD.files().begin()->first;
  const std::string original = SD.files().begin()->second;
  const size_t firstRow = original.find('\n') + 1;
  const std::string row = original.substr(firstRow);
  SD.setFileContents(duplicatePath, original + row);
  char duplicateName[EventLog::kBasenameCapacity] = {};
  assert(!duplicate.selectOldestClosedSegment(duplicateName, sizeof(duplicateName)));
  duplicate.status(corruptionStatus);
  assert(corruptionStatus.hasCorruptSegments && SD.fileCount() == 1);

  SD.clear();
  EventLog overflow;
  assert(overflow.begin(true));
  for (size_t i = 0; i < EventLog::kQueueCapacity; ++i)
    assert(overflow.enqueue(Action::Boot, Result::Success, "", "", 0,
                            static_cast<uint32_t>(i), NetworkMode::AP));
  assert(!overflow.enqueue(Action::Boot, Result::Success, "", "", 0, 99,
                           NetworkMode::AP));
  overflow.status(status);
  assert(status.queueDepth == EventLog::kQueueCapacity && status.droppedEvents == 1);

  SD.clear();
  for (size_t i = 0; i < EventLog::kMaxSegments; ++i) {
    char path[64] = {};
    std::snprintf(path, sizeof(path), "/smartlock/outbox/foreign%02u", static_cast<unsigned>(i));
    SD.setFileContents(path, "preserve");
  }
  EventLog full;
  assert(full.begin(true));
  full.status(status);
  assert(!status.outboxFull && status.segmentCount == 0 && status.hasUnrecognizedFiles);
  assert(full.enqueue(Action::Boot, Result::Success, "", "", 0, 5,
                      NetworkMode::AP));
  full.update();
  full.status(status);
  assert(status.droppedEvents == 0 && SD.fileCount() == EventLog::kMaxSegments + 1);

  SD.clear();
  EventLog unavailable;
  assert(!unavailable.begin(false));
  assert(!unavailable.enqueue(Action::Boot, Result::Success, "", "", 0, 6,
                              NetworkMode::AP));
  unavailable.status(status);
  assert(!status.enabled && status.droppedEvents == 1 && SD.fileCount() == 0);

  // Loss evidence reserves durable 64-event blocks, avoiding one NVS write
  // for every rejected event while remaining conservative after a reboot.
  SD.clear(); Preferences::clearAll();
  EventLog lossAccounting;
  assert(!lossAccounting.begin(false));
  for (size_t i = 0; i < 64; ++i)
    assert(!lossAccounting.enqueue(Action::Boot, Result::Success, "", "", 0,
                                   static_cast<uint32_t>(i), NetworkMode::AP));
  lossAccounting.status(status);
  assert(status.unrecordedEvents == 64 && Preferences::writes() == 1);
  EventLog lossReboot;
  assert(!lossReboot.begin(false));
  lossReboot.status(status);
  assert(status.unrecordedEvents == 64);
  assert(!lossReboot.enqueue(Action::Boot, Result::Success, "", "", 0, 65, NetworkMode::AP));
  lossReboot.status(status);
  assert(status.unrecordedEvents == 128 && Preferences::writes() == 2);

  SD.clear();
  EventLog lanOnly;
  assert(lanOnly.begin(true));
  assert(lanOnly.enqueue(Action::AccessGranted, Result::Success, "U000002", "D000002", 0, 7, NetworkMode::STA,
                         smartlock::events::Source::Lan, "", "A,\"=1", "OWNER", "ACTOR", "D000002", ""));
  lanOnly.update(); lanOnly.update();
  assert(lanOnly.sealCurrentSegment());
  assert(lanOnly.selectOldestClosedSegment(basename, sizeof(basename)));
  assert(lanOnly.readClosedSegment(basename, payload, sizeof(payload), payloadLength));
  assert(std::string(payload).find(",STA,7,") != std::string::npos);
  assert(std::string(payload, payloadLength).find("3,\"A,\"\"=1\",OWNER,ACTOR,D000002,,google-v1,") != std::string::npos);

  // Empty enumeration is valid; current flushed segments remain addressable.
  EventLog::SegmentInfo listed[EventLog::kMaxSegments] = {};
  size_t listedCount = 99;
  SD.clear(); Preferences::clearAll();
  EventLog retained;
  assert(retained.begin(true));
  assert(retained.enumerateSegments(listed, EventLog::kMaxSegments, listedCount) && listedCount == 0);
  for (uint32_t i = 0; i < EventLog::kMaxSegments; ++i) {
    assert(retained.enqueue(Action::Boot, Result::Success, "", "", 0, i, NetworkMode::AP));
    retained.update(); retained.update();
    assert(retained.sealCurrentSegment());
  }
  assert(retained.enumerateSegments(listed, EventLog::kMaxSegments, listedCount) && listedCount == EventLog::kMaxSegments);
  assert(listed[0].records == 1 && listed[0].counter < listed[listedCount - 1].counter);
  char namedPayload[EventLog::kSegmentBufferCapacity] = {};
  size_t namedLength = 0;
  assert(retained.readSegmentById(listed[0].id, namedPayload, sizeof(namedPayload), namedLength));
  assert(retained.readSegmentById(listed[listedCount / 2].id, namedPayload, sizeof(namedPayload), namedLength));
  assert(retained.readSegmentById(listed[listedCount - 1].id, namedPayload, sizeof(namedPayload), namedLength));
  assert(!retained.readSegmentById("../escape.csv", namedPayload, sizeof(namedPayload), namedLength));

  // Ordinary reads do not pin a segment. The oldest valid closed segment is evicted.
  char readOldest[EventLog::kBasenameCapacity] = {};
  std::memcpy(readOldest, listed[0].id, sizeof(readOldest));
  assert(retained.enqueue(Action::Boot, Result::Success, "", "", 0, 999, NetworkMode::AP));
  retained.update(); retained.update(); retained.sealCurrentSegment();
  retained.status(status);
  assert(status.segmentCount == EventLog::kMaxSegments);
  assert(status.historyLostEvents == 1 && status.historyLostSegments == 1);
  assert(!retained.readSegmentById(readOldest, namedPayload, sizeof(namedPayload), namedLength));
  assert(retained.enumerateSegments(listed, EventLog::kMaxSegments, listedCount) && listedCount == EventLog::kMaxSegments);
  char pinned[EventLog::kBasenameCapacity] = {};
  std::memcpy(pinned, listed[0].id, sizeof(pinned));
  assert(retained.pinPendingAckSegment(pinned, true));
  assert(retained.enqueue(Action::Boot, Result::Success, "", "", 0, 1000, NetworkMode::AP));
  retained.update(); retained.update(); retained.sealCurrentSegment();
  assert(retained.readSegmentById(pinned, namedPayload, sizeof(namedPayload), namedLength));
  assert(retained.pinPendingAckSegment(pinned, false));
  EventLog afterRotation;
  assert(afterRotation.begin(true));
  afterRotation.status(status);
  assert(status.historyLostEvents == 2 && status.historyLostSegments == 2);
  assert(afterRotation.enumerateSegments(listed, EventLog::kMaxSegments, listedCount) && listedCount == EventLog::kMaxSegments);
  assert(std::strcmp(listed[0].id, pinned) == 0);
  assert(afterRotation.protectPendingHistory());
  const std::string oldestBeforeProtectedFull = listed[0].id;
  assert(afterRotation.enqueue(Action::Boot, Result::Success, "", "", 0, 1001, NetworkMode::AP));
  afterRotation.update();
  afterRotation.status(status);
  assert(status.pendingHistoryProtected && status.protectedQueueFull && status.unrecordedEvents == 64);
  assert(status.historyLostEvents == 2 && status.historyLostSegments == 2);
  assert(afterRotation.enumerateSegments(listed, EventLog::kMaxSegments, listedCount));
  assert(listedCount == EventLog::kMaxSegments && oldestBeforeProtectedFull == listed[0].id);
  EventLog protectedReboot;
  assert(protectedReboot.begin(true));
  protectedReboot.status(status);
  assert(status.pendingHistoryProtected && status.unrecordedEvents == 64);
  assert(status.historyLostEvents == 2 && status.historyLostSegments == 2);

  // A validated legacy ten-column segment remains readable after upgrade.
  const size_t lineStart = firstPayload.find('\n') + 1;
  const size_t lineEnd = firstPayload.find('\n', lineStart);
  const std::string newRow = firstPayload.substr(lineStart, lineEnd - lineStart);
  size_t commas = 0, legacyBoundary = 0;
  for (; legacyBoundary < newRow.size() && commas < 9; ++legacyBoundary)
    if (newRow[legacyBoundary] == ',') ++commas;
  assert(commas == 9);
  const std::string oldRowBody = newRow.substr(0, legacyBoundary - 1);
  char checksum[9] = {};
  std::snprintf(checksum, sizeof(checksum), "%08X", static_cast<unsigned>(testCrc32(oldRowBody)));
  const std::string oldCsv = "event_id,timestamp,time_quality,user_id,device_id,action,result,network_mode,uptime_ms,crc32\n" +
                             oldRowBody + "," + checksum + "\n";
  SD.clear(); Preferences::clearAll();
  EventLog legacyReader;
  assert(legacyReader.begin(true));
  const std::string legacyPath = std::string("/smartlock/outbox/") + basename;
  SD.setFileContents(legacyPath, oldCsv);
  assert(legacyReader.readSegmentById(basename, namedPayload, sizeof(namedPayload), namedLength));
  assert(std::string(namedPayload, namedLength) == oldCsv);

  // The pre-Google 12-column schema remains readable too.
  const std::string currentBody = oldRowBody + ",,UNKNOWN";
  std::snprintf(checksum, sizeof(checksum), "%08X", static_cast<unsigned>(testCrc32(currentBody)));
  const std::string currentCsv = "event_id,timestamp,time_quality,user_id,device_id,action,result,network_mode,uptime_ms,claimed_device_id,access_source,crc32\n" +
      currentBody + "," + checksum + "\n";
  SD.clear(); Preferences::clearAll();
  EventLog currentReader;
  assert(currentReader.begin(true));
  SD.setFileContents(legacyPath, currentCsv);
  assert(currentReader.readSegmentById(basename, namedPayload, sizeof(namedPayload), namedLength));
  assert(std::string(namedPayload, namedLength) == currentCsv);

  return 0;
}
