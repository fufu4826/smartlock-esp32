#include "DeviceStore.h"

#include "AtomicFileStore.h"

#include <string.h>

namespace smartlock {
namespace storage {
namespace {

const char kPath[] = "/smartlock/db/devices.rec";

// These buffers are static to keep the ESP32 loop task stack small. Store
// operations are synchronous and assume a single caller/task; add locking if
// web handlers or other tasks begin using the stores concurrently.
uint8_t gDevicePayload[AtomicFileStore::kMaxPayload];
DeviceRecord gLoadedDevices[DeviceStore::kMaxRecords];
char gDeviceIds[DeviceStore::kMaxRecords][kIdCapacity];
char gEncodedLine[kMaxRecordLineLength + 1];

bool containsId(const char ids[][kIdCapacity], size_t count, const char* id) {
  for (size_t i = 0; i < count; ++i) {
    if (strcmp(ids[i], id) == 0) return true;
  }
  return false;
}

bool parsePayload(const uint8_t* payload, size_t length, size_t* recordCount) {
  if (recordCount == NULL || (payload == NULL && length != 0)) return false;
  *recordCount = 0;
  if (length == 0) return true;

  size_t start = 0;
  size_t count = 0;
  while (start < length) {
    size_t end = start;
    while (end < length && payload[end] != '\n') ++end;
    if (end == length || end == start || count >= DeviceStore::kMaxRecords) {
      return false;  // Missing newline, blank line, or too many records.
    }
    DeviceRecord parsed = {};
    const size_t lineLength = end - start;
    if (lineLength > kMaxRecordLineLength ||
        !decodeDeviceRecord(reinterpret_cast<const char*>(payload + start),
                            lineLength, &parsed) ||
        containsId(gDeviceIds, count, parsed.id)) {
      return false;
    }
    gLoadedDevices[count] = parsed;
    memcpy(gDeviceIds[count], parsed.id, sizeof(gDeviceIds[count]));
    ++count;
    start = end + 1;
  }
  *recordCount = count;
  return true;
}

}  // namespace

bool DeviceStore::load(DeviceRecord* records, size_t capacity, size_t& count) {
  count = 0;
  size_t payloadLength = 0;
  const AtomicFileStore::ReadResult result = AtomicFileStore::read(
      kPath, gDevicePayload, sizeof(gDevicePayload), payloadLength);
  if (result == AtomicFileStore::ReadResult::Missing) return true;
  if (result != AtomicFileStore::ReadResult::CurrentValid) return false;

  size_t parsedCount = 0;
  if (!parsePayload(gDevicePayload, payloadLength, &parsedCount) ||
      parsedCount > capacity || (parsedCount != 0 && records == NULL)) {
    return false;
  }
  if (parsedCount != 0) {
    memcpy(records, gLoadedDevices, parsedCount * sizeof(DeviceRecord));
  }
  count = parsedCount;
  return true;
}

bool DeviceStore::replace(const DeviceRecord* records, size_t count) {
  if (count > kMaxRecords || (count != 0 && records == NULL)) return false;

  size_t oldLength = 0, oldCount = 0;
  const AtomicFileStore::ReadResult oldStatus = AtomicFileStore::read(
      kPath, gDevicePayload, sizeof(gDevicePayload), oldLength);
  if (oldStatus != AtomicFileStore::ReadResult::Missing &&
      (oldStatus != AtomicFileStore::ReadResult::CurrentValid ||
       !parsePayload(gDevicePayload, oldLength, &oldCount))) return false;

  size_t used = 0;
  for (size_t i = 0; i < count; ++i) {
    size_t lineLength = 0;
    if (!encodeDeviceRecord(records[i], gEncodedLine, sizeof(gEncodedLine),
                            &lineLength) ||
        containsId(gDeviceIds, i, records[i].id) ||
        used + lineLength + 1 > sizeof(gDevicePayload)) {
      return false;
    }
    memcpy(gDeviceIds[i], records[i].id, sizeof(gDeviceIds[i]));
    memcpy(gDevicePayload + used, gEncodedLine, lineLength);
    used += lineLength;
    gDevicePayload[used++] = '\n';
  }

  // Empty payload is valid for a newly initialized store. AtomicFileStore
  // still writes its integrity header for the committed empty file.
  return AtomicFileStore::write(kPath, gDevicePayload, used);
}

bool DeviceStore::validate() {
  size_t length = 0, count = 0;
  const AtomicFileStore::ReadResult result = AtomicFileStore::read(
      kPath, gDevicePayload, sizeof(gDevicePayload), length);
  return result == AtomicFileStore::ReadResult::Missing ||
         (result == AtomicFileStore::ReadResult::CurrentValid &&
          parsePayload(gDevicePayload, length, &count));
}

}  // namespace storage
}  // namespace smartlock
