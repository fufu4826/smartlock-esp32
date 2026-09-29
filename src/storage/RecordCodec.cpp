#include "RecordCodec.h"

#include <string.h>

namespace smartlock {
namespace storage {
namespace {

struct Field {
  const char* data;
  size_t length;
};

bool boundedLength(const char* value, size_t capacity, size_t* length) {
  if (value == NULL || length == NULL) return false;
  for (size_t i = 0; i < capacity; ++i) {
    if (value[i] == '\0') {
      *length = i;
      return true;
    }
  }
  return false;
}

bool validId(const char* value, size_t length) {
  if (value == NULL || length == 0 || length > kIdCapacity - 1) return false;
  for (size_t i = 0; i < length; ++i) {
    const char c = value[i];
    const bool alpha = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    const bool digit = c >= '0' && c <= '9';
    if (!alpha && !digit && c != '_' && c != '-') return false;
  }
  return true;
}

bool validUtf8Name(const char* value, size_t length) {
  if (value == NULL || length == 0 || length > kNameCapacity - 1) return false;
  size_t i = 0;
  while (i < length) {
    const uint8_t a = static_cast<uint8_t>(value[i]);
    uint32_t codePoint = 0;
    size_t extra = 0;
    if (a <= 0x7F) {
      codePoint = a;
      if (a < 0x20 || a == 0x7F || a == '|') return false;
    } else if (a >= 0xC2 && a <= 0xDF) {
      codePoint = a & 0x1F;
      extra = 1;
    } else if (a >= 0xE0 && a <= 0xEF) {
      codePoint = a & 0x0F;
      extra = 2;
    } else if (a >= 0xF0 && a <= 0xF4) {
      codePoint = a & 0x07;
      extra = 3;
    } else {
      return false;
    }
    if (i + extra >= length) return false;
    for (size_t j = 1; j <= extra; ++j) {
      const uint8_t b = static_cast<uint8_t>(value[i + j]);
      if ((b & 0xC0) != 0x80) return false;
      if (j == 1) {
        if (a == 0xE0 && b < 0xA0) return false;  // Overlong encoding.
        if (a == 0xED && b >= 0xA0) return false; // UTF-16 surrogate.
        if (a == 0xF0 && b < 0x90) return false;  // Overlong encoding.
        if (a == 0xF4 && b >= 0x90) return false; // Above U+10FFFF.
      }
      codePoint = (codePoint << 6) | (b & 0x3F);
    }
    if (codePoint >= 0x80 && codePoint <= 0x9F) return false;
    i += extra + 1;
  }
  return true;
}

const char* roleText(UserRole role) {
  switch (role) {
    case UserRole::Owner: return "OWNER";
    case UserRole::Admin: return "ADMIN";
    case UserRole::User: return "USER";
    case UserRole::Guest: return "GUEST";
  }
  return NULL;
}

bool parseRole(const Field& field, UserRole* role) {
  if (field.length == 5 && memcmp(field.data, "OWNER", 5) == 0) {
    *role = UserRole::Owner;
  } else if (field.length == 5 && memcmp(field.data, "ADMIN", 5) == 0) {
    *role = UserRole::Admin;
  } else if (field.length == 4 && memcmp(field.data, "USER", 4) == 0) {
    *role = UserRole::User;
  } else if (field.length == 5 && memcmp(field.data, "GUEST", 5) == 0) {
    *role = UserRole::Guest;
  } else {
    return false;
  }
  return true;
}

const char* statusText(RecordStatus status) {
  switch (status) {
    case RecordStatus::Active: return "ACTIVE";
    case RecordStatus::Revoked: return "REVOKED";
  }
  return NULL;
}

bool parseStatus(const Field& field, RecordStatus* status) {
  if (field.length == 6 && memcmp(field.data, "ACTIVE", 6) == 0) {
    *status = RecordStatus::Active;
  } else if (field.length == 7 && memcmp(field.data, "REVOKED", 7) == 0) {
    *status = RecordStatus::Revoked;
  } else {
    return false;
  }
  return true;
}

bool parseHex32(const Field& field, uint32_t* value) {
  if (field.length != 8 || value == NULL) return false;
  uint32_t result = 0;
  for (size_t i = 0; i < field.length; ++i) {
    const char c = field.data[i];
    uint8_t nibble;
    if (c >= '0' && c <= '9') nibble = static_cast<uint8_t>(c - '0');
    else if (c >= 'A' && c <= 'F') nibble = static_cast<uint8_t>(c - 'A' + 10);
    else return false;
    result = (result << 4) | nibble;
  }
  *value = result;
  return true;
}

void writeHex32(uint32_t value, char* output) {
  static const char kHex[] = "0123456789ABCDEF";
  for (int i = 7; i >= 0; --i) {
    output[i] = kHex[value & 0x0F];
    value >>= 4;
  }
}

bool append(char* buffer, size_t capacity, size_t* used,
            const char* value, size_t length) {
  if (buffer == NULL || used == NULL || value == NULL ||
      length > capacity - *used) return false;
  memcpy(buffer + *used, value, length);
  *used += length;
  return true;
}

bool appendField(char* buffer, size_t capacity, size_t* used,
                 const char* value, size_t length) {
  return append(buffer, capacity, used, "|", 1) &&
         append(buffer, capacity, used, value, length);
}

bool finishEncoded(char* temporary, size_t capacity, size_t* used,
                   char* output, size_t outputCapacity,
                   size_t* outputLength) {
  if (*used + 1 + 8 + 1 > capacity ||
      *used + 1 + 8 + 1 > outputCapacity) {
    return false;
  }
  temporary[(*used)++] = '|';
  const uint32_t crc = recordCrc32(
      reinterpret_cast<const uint8_t*>(temporary), *used);
  writeHex32(crc, temporary + *used);
  *used += 8;
  temporary[*used] = '\0';
  memcpy(output, temporary, *used + 1);
  if (outputLength != NULL) *outputLength = *used;
  return true;
}

bool splitFields(const char* line, size_t length, Field* fields,
                 size_t expectedCount) {
  if (line == NULL || fields == NULL || length == 0 ||
      length > kMaxRecordLineLength) return false;
  size_t fieldIndex = 0;
  size_t start = 0;
  for (size_t i = 0; i <= length; ++i) {
    if (i == length || line[i] == '|') {
      if (fieldIndex >= expectedCount || i == start) return false;
      fields[fieldIndex].data = line + start;
      fields[fieldIndex].length = i - start;
      ++fieldIndex;
      start = i + 1;
    } else if (static_cast<uint8_t>(line[i]) < 0x20 || line[i] == '\x7F') {
      return false;
    }
  }
  return fieldIndex == expectedCount;
}

bool checkTypeVersion(const Field& field, char type) {
  return field.length == 2 && field.data[0] == type &&
         field.data[1] == '1' && kSchemaVersion == 1;
}

bool verifyCrc(const char* line, size_t length, const Field& crcField) {
  uint32_t stored = 0;
  if (!parseHex32(crcField, &stored)) return false;
  const size_t prefixLength =
      static_cast<size_t>(crcField.data - line);
  return recordCrc32(reinterpret_cast<const uint8_t*>(line), prefixLength) ==
         stored;
}

void copyField(char* destination, size_t capacity, const Field& field) {
  memcpy(destination, field.data, field.length);
  destination[field.length] = '\0';
  (void)capacity;
}

}  // namespace

uint32_t recordCrc32(const uint8_t* bytes, size_t length) {
  if (bytes == NULL && length != 0) return 0;
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0; i < length; ++i) {
    crc ^= bytes[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 1U) ? ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
    }
  }
  return crc ^ 0xFFFFFFFFUL;
}

bool encodeUserRecord(const UserRecord& record, char* output,
                      size_t capacity, size_t* outputLength) {
  size_t idLength, nameLength;
  const char* role = roleText(record.role);
  const char* status = statusText(record.status);
  if (output == NULL || !boundedLength(record.id, sizeof(record.id), &idLength) ||
      !boundedLength(record.name, sizeof(record.name), &nameLength) ||
      !validId(record.id, idLength) ||
      !validUtf8Name(record.name, nameLength) || role == NULL ||
      status == NULL) return false;
  char temporary[kMaxRecordLineLength + 1];
  size_t used = 0;
  const size_t roleLength = strlen(role);
  const size_t statusLength = strlen(status);
  return append(temporary, sizeof(temporary), &used, "U1|", 3) &&
         append(temporary, sizeof(temporary), &used, record.id, idLength) &&
         appendField(temporary, sizeof(temporary), &used, record.name, nameLength) &&
         appendField(temporary, sizeof(temporary), &used, role, roleLength) &&
         appendField(temporary, sizeof(temporary), &used, status, statusLength) &&
         finishEncoded(temporary, sizeof(temporary), &used, output, capacity,
                       outputLength);
}

bool decodeUserRecord(const char* line, size_t length, UserRecord* output) {
  if (output == NULL) return false;
  Field fields[6];
  if (!splitFields(line, length, fields, 6) ||
      !checkTypeVersion(fields[0], 'U') ||
      !validId(fields[1].data, fields[1].length) ||
      !validUtf8Name(fields[2].data, fields[2].length) ||
      !verifyCrc(line, length, fields[5])) return false;
  UserRecord parsed = {};
  if (!parseRole(fields[3], &parsed.role) ||
      !parseStatus(fields[4], &parsed.status)) return false;
  copyField(parsed.id, sizeof(parsed.id), fields[1]);
  copyField(parsed.name, sizeof(parsed.name), fields[2]);
  *output = parsed;
  return true;
}

bool encodeIdentityRecord(const IdentityRecord& record, char* output,
                      size_t capacity, size_t* outputLength) {
  size_t idLength, nameLength;
  const char* role = roleText(record.role);
  const char* status = statusText(record.status);
  if (output == NULL || !boundedLength(record.id, sizeof(record.id), &idLength) ||
      !boundedLength(record.name, sizeof(record.name), &nameLength) ||
      !validId(record.id, idLength) ||
      !validUtf8Name(record.name, nameLength) || role == NULL ||
      status == NULL) return false;
  char temporary[kMaxRecordLineLength + 1];
  size_t used = 0;
  const size_t roleLength = strlen(role);
  const size_t statusLength = strlen(status);
  return append(temporary, sizeof(temporary), &used, "I1|", 3) &&
         append(temporary, sizeof(temporary), &used, record.id, idLength) &&
         appendField(temporary, sizeof(temporary), &used, record.name, nameLength) &&
         appendField(temporary, sizeof(temporary), &used, role, roleLength) &&
         appendField(temporary, sizeof(temporary), &used, status, statusLength) &&
         finishEncoded(temporary, sizeof(temporary), &used, output, capacity,
                       outputLength);
}

bool decodeIdentityRecord(const char* line, size_t length, IdentityRecord* output) {
  if (output == NULL) return false;
  Field fields[6];
  if (!splitFields(line, length, fields, 6) ||
      !checkTypeVersion(fields[0], 'I') ||
      !validId(fields[1].data, fields[1].length) ||
      !validUtf8Name(fields[2].data, fields[2].length) ||
      !verifyCrc(line, length, fields[5])) return false;
  IdentityRecord parsed = {};
  if (!parseRole(fields[3], &parsed.role) ||
      !parseStatus(fields[4], &parsed.status)) return false;
  copyField(parsed.id, sizeof(parsed.id), fields[1]);
  copyField(parsed.name, sizeof(parsed.name), fields[2]);
  *output = parsed;
  return true;
}

bool encodeDeviceRecord(const DeviceRecord& record, char* output,
                        size_t capacity, size_t* outputLength) {
  size_t idLength, userIdLength, nameLength;
  const char* status = statusText(record.status);
  if (output == NULL || !boundedLength(record.id, sizeof(record.id), &idLength) ||
      !boundedLength(record.userId, sizeof(record.userId), &userIdLength) ||
      !boundedLength(record.name, sizeof(record.name), &nameLength) ||
      !validId(record.id, idLength) || !validId(record.userId, userIdLength) ||
      !validUtf8Name(record.name, nameLength) || status == NULL) return false;
  char temporary[kMaxRecordLineLength + 1];
  size_t used = 0;
  const size_t statusLength = strlen(status);
  return append(temporary, sizeof(temporary), &used, "D1|", 3) &&
         append(temporary, sizeof(temporary), &used, record.id, idLength) &&
         appendField(temporary, sizeof(temporary), &used,
                     record.userId, userIdLength) &&
         appendField(temporary, sizeof(temporary), &used,
                     record.name, nameLength) &&
         appendField(temporary, sizeof(temporary), &used,
                     status, statusLength) &&
         finishEncoded(temporary, sizeof(temporary), &used, output, capacity,
                       outputLength);
}

bool decodeDeviceRecord(const char* line, size_t length,
                        DeviceRecord* output) {
  if (output == NULL) return false;
  Field fields[6];
  if (!splitFields(line, length, fields, 6) ||
      !checkTypeVersion(fields[0], 'D') ||
      !validId(fields[1].data, fields[1].length) ||
      !validId(fields[2].data, fields[2].length) ||
      !validUtf8Name(fields[3].data, fields[3].length) ||
      !verifyCrc(line, length, fields[5])) return false;
  DeviceRecord parsed = {};
  if (!parseStatus(fields[4], &parsed.status)) return false;
  copyField(parsed.id, sizeof(parsed.id), fields[1]);
  copyField(parsed.userId, sizeof(parsed.userId), fields[2]);
  copyField(parsed.name, sizeof(parsed.name), fields[3]);
  *output = parsed;
  return true;
}

}  // namespace storage
}  // namespace smartlock
