#include "AtomicFileStore.h"

#include <SD.h>
#include <string.h>

uint32_t AtomicFileStore::crc32(const uint8_t* bytes, size_t length) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < length; ++i) {
    crc ^= bytes[i];
    for (int bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

bool AtomicFileStore::readOne(const char* path, uint8_t* output,
                              size_t capacity, size_t& length) {
  length = 0;
  if (!SD.exists(path)) return false;
  File file = SD.open(path, FILE_READ);
  if (!file) return false;
  Header header = {};
  const bool headerOk = file.readBytes(reinterpret_cast<char*>(&header), sizeof(header)) == sizeof(header);
  if (!headerOk || memcmp(header.magic, "SLDB", 4) != 0 || header.schema != 1 ||
      header.length > kMaxPayload || header.length > capacity ||
      file.size() != sizeof(header) + header.length) {
    file.close();
    return false;
  }
  const size_t got = file.readBytes(reinterpret_cast<char*>(output), header.length);
  file.close();
  if (got != header.length || crc32(output, got) != header.crc32) return false;
  length = got;
  return true;
}

AtomicFileStore::ReadResult AtomicFileStore::read(
    const char* path, uint8_t* output, size_t capacity, size_t& length) {
  length = 0;
  if (readOne(path, output, capacity, length)) return ReadResult::CurrentValid;
  char backup[96], temp[96];
  if (snprintf(backup, sizeof(backup), "%s.bak", path) >= static_cast<int>(sizeof(backup)))
    return ReadResult::Corrupt;
  if (snprintf(temp, sizeof(temp), "%s.tmp", path) >= static_cast<int>(sizeof(temp)))
    return ReadResult::Corrupt;
  const bool currentExists = SD.exists(path);
  if (readOne(backup, output, capacity, length)) return ReadResult::BackupOnly;
  return (!currentExists && !SD.exists(backup) && !SD.exists(temp))
             ? ReadResult::Missing : ReadResult::Corrupt;
}

bool AtomicFileStore::write(const char* path, const uint8_t* payload, size_t length) {
  if (path == nullptr || payload == nullptr || length > kMaxPayload || length > UINT16_MAX)
    return false;
  const size_t pathLength = strlen(path);
  if (pathLength < 15 || pathLength > 90 ||
      strncmp(path, "/smartlock/db/", 14) != 0 ||
      strstr(path, "..") != nullptr) return false;
  char temp[96], backup[96];
  if (snprintf(temp, sizeof(temp), "%s.tmp", path) >= static_cast<int>(sizeof(temp)) ||
      snprintf(backup, sizeof(backup), "%s.bak", path) >= static_cast<int>(sizeof(backup)))
    return false;

  // Static scratch avoids overflowing the ESP32 loop task stack. All storage
  // operations currently run synchronously on that task; add locking before
  // allowing concurrent web handlers to call this class.
  static uint8_t scratch[kMaxPayload];
  size_t existingLength = 0;
  const ReadResult status = read(path, scratch, sizeof(scratch), existingLength);
  if (status == ReadResult::Corrupt || status == ReadResult::BackupOnly) return false;
  if (SD.exists(temp) && !SD.remove(temp)) return false;

  Header header = {{'S', 'L', 'D', 'B'}, 1, static_cast<uint16_t>(length), crc32(payload, length)};
  File file = SD.open(temp, FILE_WRITE);
  if (!file) return false;
  const bool written = file.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) == sizeof(header) &&
                       file.write(payload, length) == length;
  file.flush();
  file.close();
  if (!written) return false;
  size_t checkLength = 0;
  if (!readOne(temp, scratch, sizeof(scratch), checkLength) ||
      checkLength != length || memcmp(scratch, payload, length) != 0) return false;

  if (status == ReadResult::CurrentValid) {
    if (SD.exists(backup) && !SD.remove(backup)) return false;
    if (!SD.rename(path, backup)) return false;
  }
  if (!SD.rename(temp, path)) return false;
  return true;
}

bool AtomicFileStore::repairUnconfiguredSetupFile(const char* path) {
  if (!path || (strcmp(path, "/smartlock/db/users.rec") != 0 &&
                strcmp(path, "/smartlock/db/devices.rec") != 0 &&
                strcmp(path, "/smartlock/db/auth.rec") != 0)) return false;
  static uint8_t recoveryScratch[kMaxPayload];
  size_t length = 0;
  if (readOne(path, recoveryScratch, sizeof(recoveryScratch), length)) return true;
  if (SD.exists(path)) return false; // A corrupt committed file is not repaired here.
  char temp[96], backup[96];
  if (snprintf(temp, sizeof(temp), "%s.tmp", path) >= static_cast<int>(sizeof(temp)) ||
      snprintf(backup, sizeof(backup), "%s.bak", path) >= static_cast<int>(sizeof(backup)))
    return false;
  if (!SD.exists(temp) && !SD.exists(backup)) return true;
  if (readOne(temp, recoveryScratch, sizeof(recoveryScratch), length))
    return SD.rename(temp, path);
  if (readOne(backup, recoveryScratch, sizeof(recoveryScratch), length))
    return SD.rename(backup, path);
  return false;
}
