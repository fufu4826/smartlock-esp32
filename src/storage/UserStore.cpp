#include "UserStore.h"

#include "AtomicFileStore.h"

#include <string.h>

namespace smartlock {
namespace storage {
namespace {

const char kPath[] = "/smartlock/db/users.rec";

// These buffers are static to keep the ESP32 loop task stack small. Store
// operations are synchronous and assume a single caller/task; add locking if
// web handlers or other tasks begin using the stores concurrently.
uint8_t gUserPayload[AtomicFileStore::kMaxPayload];
UserRecord gLoadedUsers[UserStore::kMaxRecords];
char gUserIds[UserStore::kMaxRecords][kIdCapacity];
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
    if (end == length || end == start || count >= UserStore::kMaxRecords) {
      return false;  // Missing newline, blank line, or too many records.
    }
    UserRecord parsed = {};
    const size_t lineLength = end - start;
    if (lineLength > kMaxRecordLineLength ||
        !decodeUserRecord(reinterpret_cast<const char*>(payload + start),
                          lineLength, &parsed) ||
        containsId(gUserIds, count, parsed.id)) {
      return false;
    }
    gLoadedUsers[count] = parsed;
    memcpy(gUserIds[count], parsed.id, sizeof(gUserIds[count]));
    ++count;
    start = end + 1;
  }
  *recordCount = count;
  return true;
}

}  // namespace

bool UserStore::load(UserRecord* records, size_t capacity, size_t& count) {
  count = 0;
  size_t payloadLength = 0;
  const AtomicFileStore::ReadResult result = AtomicFileStore::read(
      kPath, gUserPayload, sizeof(gUserPayload), payloadLength);
  if (result == AtomicFileStore::ReadResult::Missing) return true;
  if (result != AtomicFileStore::ReadResult::CurrentValid) return false;

  size_t parsedCount = 0;
  if (!parsePayload(gUserPayload, payloadLength, &parsedCount) ||
      parsedCount > capacity || (parsedCount != 0 && records == NULL)) {
    return false;
  }
  if (parsedCount != 0) {
    memcpy(records, gLoadedUsers, parsedCount * sizeof(UserRecord));
  }
  count = parsedCount;
  return true;
}

bool UserStore::replace(const UserRecord* records, size_t count) {
  if (count > kMaxRecords || (count != 0 && records == NULL)) return false;

  // Validate the committed logical database before replacing it. The atomic
  // envelope alone cannot detect a correctly checksummed but malformed record.
  size_t oldLength = 0, oldCount = 0;
  const AtomicFileStore::ReadResult oldStatus = AtomicFileStore::read(
      kPath, gUserPayload, sizeof(gUserPayload), oldLength);
  if (oldStatus != AtomicFileStore::ReadResult::Missing &&
      (oldStatus != AtomicFileStore::ReadResult::CurrentValid ||
       !parsePayload(gUserPayload, oldLength, &oldCount))) return false;

  size_t used = 0;
  for (size_t i = 0; i < count; ++i) {
    size_t lineLength = 0;
    if (!encodeUserRecord(records[i], gEncodedLine, sizeof(gEncodedLine),
                          &lineLength) ||
        containsId(gUserIds, i, records[i].id) ||
        used + lineLength + 1 > sizeof(gUserPayload)) {
      return false;
    }
    memcpy(gUserIds[i], records[i].id, sizeof(gUserIds[i]));
    memcpy(gUserPayload + used, gEncodedLine, lineLength);
    used += lineLength;
    gUserPayload[used++] = '\n';
  }

  // Empty payload is valid for a newly initialized store. AtomicFileStore
  // still writes its integrity header for the committed empty file.
  return AtomicFileStore::write(kPath, gUserPayload, used);
}

bool UserStore::validate() {
  size_t length = 0, count = 0;
  const AtomicFileStore::ReadResult result = AtomicFileStore::read(
      kPath, gUserPayload, sizeof(gUserPayload), length);
  return result == AtomicFileStore::ReadResult::Missing ||
         (result == AtomicFileStore::ReadResult::CurrentValid &&
          parsePayload(gUserPayload, length, &count));
}

}  // namespace storage
}  // namespace smartlock
