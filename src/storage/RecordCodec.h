#pragma once

#include <stddef.h>
#include <stdint.h>

namespace smartlock {
namespace storage {

// Record text is UTF-8, but bounded by bytes. Delimiters and control characters
// are rejected, so fields need no escaping and the encoded line is deterministic.
static const size_t kIdCapacity = 9;       // 1..8 ASCII ID bytes plus NUL
static const size_t kNameCapacity = 41;    // 1..40 UTF-8 bytes plus NUL
static const size_t kMaxRecordLineLength = 78;  // Excludes a trailing NUL/newline
static const uint8_t kSchemaVersion = 1;

enum class UserRole : uint8_t {
  Owner,
  Admin,
  User,
  Guest
};

enum class RecordStatus : uint8_t {
  Active,
  Revoked
};

struct UserRecord {
  char id[kIdCapacity];
  char name[kNameCapacity];
  UserRole role;
  RecordStatus status;
};

struct DeviceRecord {
  char id[kIdCapacity];
  char userId[kIdCapacity];
  char name[kNameCapacity];
  RecordStatus status;
};

// One browser credential maps directly to this identity. No human User key.
struct IdentityRecord {
  char id[kIdCapacity];
  char name[kNameCapacity];
  UserRole role;
  RecordStatus status;
};
bool encodeIdentityRecord(const IdentityRecord&, char*, size_t, size_t*);
bool decodeIdentityRecord(const char*, size_t, IdentityRecord*);

// Encoded forms are U1|id|name|ROLE|STATUS|CRC32 and
// D1|id|userId|name|STATUS|CRC32. CRC32 is IEEE/ISO-HDLC over all bytes
// through the final '|' before the eight uppercase hexadecimal CRC digits.
// Neither function appends a newline. On failure, output records are untouched.
bool encodeUserRecord(const UserRecord& record, char* output, size_t capacity,
                      size_t* outputLength);
bool decodeUserRecord(const char* line, size_t length, UserRecord* output);
bool encodeDeviceRecord(const DeviceRecord& record, char* output,
                        size_t capacity, size_t* outputLength);
bool decodeDeviceRecord(const char* line, size_t length,
                        DeviceRecord* output);

uint32_t recordCrc32(const uint8_t* bytes, size_t length);

}  // namespace storage
}  // namespace smartlock
