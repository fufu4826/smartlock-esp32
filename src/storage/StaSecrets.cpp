#include "StaSecrets.h"
#include "../security/RequestPolicy.h"
#include <Preferences.h>
#include <stddef.h>
#include <string.h>

namespace {
constexpr uint32_t kMagic = 0x41545353u; // SSTA
constexpr char kNamespace[] = "sl-sta";
constexpr char kSlotA[] = "sta-a";
constexpr char kSlotB[] = "sta-b";
}

uint32_t StaSecrets::crc32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xffffffffu;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (int b = 0; b < 8; ++b)
      crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

bool StaSecrets::validInput(const char* ssid, const char* password) {
  if (!ssid || !password) return false;
  const size_t sn = strnlen(ssid, 33), pn = strnlen(password, 65);
  if (!RequestPolicy::utf8Text(ssid,33) || sn == 0 || sn > 32 || pn > 63 || (pn != 0 && pn < 8)) return false;
  for (size_t i = 0; i < sn; ++i) {
    const uint8_t c = static_cast<uint8_t>(ssid[i]);
    if (c < 0x20 || c == 0x7f) return false;
  }
  for (size_t i = 0; i < pn; ++i)
    if (password[i] < 0x20 || password[i] > 0x7e) return false;
  return true;
}

StaSecrets::Result StaSecrets::load(char (&ssid)[33], char (&password)[65]) {
  memset(ssid, 0, sizeof(ssid)); memset(password, 0, sizeof(password));
  Preferences nvs;
  // Opening read-write creates an empty namespace on first boot without the
  // misleading NVS NOT_FOUND diagnostic; no confirmed record is changed.
  if (!nvs.begin(kNamespace, false)) return Result::Invalid;
  Record a = {}, b = {};
  const bool hasA = nvs.isKey(kSlotA), hasB = nvs.isKey(kSlotB);
  const bool validA = readSlot(nvs, kSlotA, a), validB = readSlot(nvs, kSlotB, b);
  nvs.end();
  if (!validA && !validB) return hasA || hasB ? Result::Invalid : Result::Missing;
  const Record& record = validA && (!validB || a.sequence >= b.sequence) ? a : b;
  memcpy(ssid, record.ssid, sizeof(record.ssid));
  memcpy(password, record.password, sizeof(record.password));
  memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b));
  return Result::Valid;
}

bool StaSecrets::readSlot(Preferences& nvs, const char* key, Record& record) {
  if (!nvs.isKey(key) || nvs.getBytesLength(key) != sizeof(record) ||
      nvs.getBytes(key, &record, sizeof(record)) != sizeof(record)) return false;
  return record.magic == kMagic && record.schema == 1 && record.sequence != 0 &&
      validInput(record.ssid, record.password) &&
      crc32((const uint8_t*)&record, offsetof(Record, crc32)) == record.crc32;
}

bool StaSecrets::saveConfirmed(const char* ssid, const char* password) {
  if (!validInput(ssid, password)) return false;
  Preferences nvs;
  if (!nvs.begin(kNamespace, false)) return false;
  Record a = {}, b = {};
  const bool validA = readSlot(nvs, kSlotA, a), validB = readSlot(nvs, kSlotB, b);
  const char* target = !validA || (validB && a.sequence <= b.sequence) ? kSlotA : kSlotB;
  const uint32_t current = validA && validB ? (a.sequence > b.sequence ? a.sequence : b.sequence)
                           : validA ? a.sequence : validB ? b.sequence : 0;
  if (current == UINT32_MAX) { nvs.end(); return false; }
  Record record = {};
  record.magic = kMagic; record.schema = 1; record.sequence = current + 1;
  memcpy(record.ssid, ssid, strlen(ssid) + 1);
  memcpy(record.password, password, strlen(password) + 1);
  record.crc32 = crc32((const uint8_t*)&record, offsetof(Record, crc32));
  const bool wrote = nvs.putBytes(target, &record, sizeof(record)) == sizeof(record);
  Record verifiedRecord = {};
  const bool verified = wrote && readSlot(nvs, target, verifiedRecord) &&
      verifiedRecord.sequence == record.sequence &&
      strcmp(verifiedRecord.ssid, ssid) == 0 &&
      strcmp(verifiedRecord.password, password) == 0;
  nvs.end();
  memset(&record, 0, sizeof(record));
  memset(&verifiedRecord, 0, sizeof(verifiedRecord));
  memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b));
  return verified;
}
