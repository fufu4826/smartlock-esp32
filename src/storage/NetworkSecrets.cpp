#include "NetworkSecrets.h"

#include <stddef.h>
#include <string.h>

namespace {
constexpr uint32_t kMagic = 0x54454e53u; // SNET, schema checked separately
constexpr char kNamespace[] = "sl-net";
constexpr char kKey[] = "ap-pass";
}

uint32_t NetworkSecrets::crc32(const uint8_t* bytes, size_t length) {
  uint32_t crc = 0xffffffffu;
  for (size_t i = 0; i < length; ++i) {
    crc ^= bytes[i];
    for (int bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

bool NetworkSecrets::validPassword(const char* password) {
  if (!password || strnlen(password, 33) != 32) return false;
  for (size_t i = 0; i < 32; ++i) {
    const char c = password[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

NetworkSecrets::Result NetworkSecrets::load(char (&password)[33]) {
  memset(password, 0, sizeof(password));
  Preferences nvs;
  if (!nvs.begin(kNamespace, true)) return Result::Invalid;
  if (!nvs.isKey(kKey)) {
    nvs.end();
    return Result::Missing;
  }
  Record record = {};
  const bool read = nvs.getBytesLength(kKey) == sizeof(record) &&
                    nvs.getBytes(kKey, &record, sizeof(record)) == sizeof(record);
  nvs.end();
  if (!read || record.magic != kMagic || record.schema != 1 ||
      !validPassword(record.apPassword) ||
      crc32(reinterpret_cast<const uint8_t*>(&record), offsetof(Record, crc32)) != record.crc32)
    return Result::Invalid;
  memcpy(password, record.apPassword, sizeof(record.apPassword));
  return Result::Valid;
}

bool NetworkSecrets::saveSetupPassword(const char* password) {
  if (!validPassword(password)) return false;
  Record record = {};
  record.magic = kMagic;
  record.schema = 1;
  memcpy(record.apPassword, password, 33);
  record.crc32 = crc32(reinterpret_cast<const uint8_t*>(&record), offsetof(Record, crc32));
  Preferences nvs;
  if (!nvs.begin(kNamespace, false)) return false;
  const bool wrote = nvs.putBytes(kKey, &record, sizeof(record)) == sizeof(record);
  nvs.end();
  if (!wrote) return false;
  char readback[33] = {};
  return load(readback) == Result::Valid && memcmp(readback, password, 33) == 0;
}
