#include "AuthStore.h"

#include "AtomicFileStore.h"
#include "../security/RequestPolicy.h"
#include <esp_system.h>
#include <esp_wifi.h>
#include <mbedtls/sha256.h>
#include <string.h>

namespace {

constexpr uint32_t kMagic = 0x48545541u; // AUTH in little endian
uint8_t scratch[AtomicFileStore::kMaxPayload];

bool constantEqual(const uint8_t* a, const uint8_t* b, size_t length) {
  uint8_t different = 0;
  for (size_t i = 0; i < length; ++i) different |= a[i] ^ b[i];
  return different == 0;
}

int nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

bool radioReady() {
  wifi_mode_t mode = WIFI_MODE_NULL;
  return esp_wifi_get_mode(&mode) == ESP_OK && mode != WIFI_MODE_NULL;
}
}

AuthStore::DeviceAuth AuthStore::entries_[AuthStore::kMaxDevices] = {};

bool AuthStore::ownerVerifierMatches(const char* backupPath) {
  Header current = {}; ReadResult result;
  if (!read(current, entries_, kMaxDevices, result) || result != ReadResult::Valid) return false;
  DeviceAuth owner = {}; bool found = false;
  for (size_t i = 0; i < current.count; ++i) if (!strcmp(entries_[i].id, "D000001")) {
    owner = entries_[i]; found = true;
  }
  size_t length = 0;
  if (!found || !backupPath || AtomicFileStore::read(backupPath, scratch, sizeof(scratch), length) !=
      AtomicFileStore::ReadResult::CurrentValid || length < sizeof(Header)) return false;
  Header backup = {}; memcpy(&backup, scratch, sizeof(backup));
  if (backup.magic != kMagic || backup.schema != 2 ||
      !backup.count || backup.count > kMaxDevices || length != sizeof(Header) + backup.count * sizeof(DeviceAuth)) return false;
  for (size_t i = 0; i < backup.count; ++i) {
    DeviceAuth entry = {}; memcpy(&entry, scratch + sizeof(Header) + i * sizeof(entry), sizeof(entry));
    if (!memcmp(entry.id, owner.id, sizeof(entry.id)))
      return constantEqual(entry.salt, owner.salt, 16) && constantEqual(entry.hash, owner.hash, 32);
  }
  return false;
}

bool AuthStore::decodeCredential(const char* hex, uint8_t (&bytes)[32]) {
  if (!hex || strnlen(hex, 65) != 64) return false;
  for (size_t i = 0; i < 32; ++i) {
    const int high = nibble(hex[i * 2]);
    const int low = nibble(hex[i * 2 + 1]);
    if (high < 0 || low < 0) return false;
    bytes[i] = static_cast<uint8_t>((high << 4) | low);
  }
  return true;
}

bool AuthStore::read(Header& header, DeviceAuth* entries_, size_t capacity,
                     ReadResult& result) {
  result = ReadResult::Invalid;
  size_t length = 0;
  const auto status = AtomicFileStore::read("/smartlock/db/auth.rec", scratch, sizeof(scratch), length);
  if (status == AtomicFileStore::ReadResult::Missing) {
    result = ReadResult::Missing;
    return true;
  }
  if (status != AtomicFileStore::ReadResult::CurrentValid || length < sizeof(Header)) return false;
  memcpy(&header, scratch, sizeof(Header));
  if (header.magic != kMagic || header.schema != 2 || header.count == 0 ||
      header.count > kMaxDevices || header.count > capacity ||
      length != sizeof(Header) + header.count * sizeof(DeviceAuth)) return false;
  memcpy(entries_, scratch + sizeof(Header), header.count * sizeof(DeviceAuth));
  for (size_t i = 0; i < header.count; ++i) {
    if (strnlen(entries_[i].id, sizeof(entries_[i].id)) != 7 ||
        !RequestPolicy::deviceId(entries_[i].id)) return false;
    for (size_t j = 0; j < i; ++j) {
      if (strcmp(entries_[i].id, entries_[j].id) == 0) return false;
    }
  }
  result = ReadResult::Valid;
  return true;
}

AuthStore::ReadResult AuthStore::inspect(size_t& deviceCount) {
  deviceCount = 0;
  Header header = {};

  memset(entries_, 0, sizeof(entries_));
  ReadResult result;
  if (!read(header, entries_, kMaxDevices, result)) return ReadResult::Invalid;
  if (result == ReadResult::Valid) deviceCount = header.count;
  return result;
}

bool AuthStore::hasDevice(const char* deviceId) {
  if (!deviceId) return false;
  Header header = {};

  memset(entries_, 0, sizeof(entries_));
  ReadResult result;
  if (!read(header, entries_, kMaxDevices, result) || result != ReadResult::Valid) return false;
  for (size_t i = 0; i < header.count; ++i) {
    if (strcmp(entries_[i].id, deviceId) == 0) return true;
  }
  return false;
}

bool AuthStore::saveFirst(const char* deviceId, const char* credentialHex) {
  if (!deviceId || strcmp(deviceId, "D000001") != 0 || !radioReady()) return false;
  uint8_t credential[32] = {};
  if (!decodeCredential(credentialHex, credential)) return false;
  Header oldHeader = {};

  memset(entries_, 0, sizeof(entries_));
  ReadResult previous;
  if (!read(oldHeader, entries_, kMaxDevices, previous) || previous != ReadResult::Missing) return false;

  Header header = {};
  header.magic = kMagic;
  header.schema = 2;
  header.count = 1;
  DeviceAuth entry = {};
  memcpy(entry.id, deviceId, strlen(deviceId) + 1);
  esp_fill_random(entry.salt, sizeof(entry.salt));
  uint8_t input[48] = {};
  memcpy(input, entry.salt, 16);
  memcpy(input + 16, credential, 32);
  const bool hashOkay = mbedtls_sha256_ret(input, sizeof(input), entry.hash, 0) == 0;
  memset(input, 0, sizeof(input));
  memset(credential, 0, sizeof(credential));
  if (!hashOkay) return false;
  uint8_t payload[sizeof(Header) + sizeof(DeviceAuth)] = {};
  memcpy(payload, &header, sizeof(header));
  memcpy(payload + sizeof(header), &entry, sizeof(entry));
  const bool written = AtomicFileStore::write("/smartlock/db/auth.rec", payload, sizeof(payload));
  memset(payload, 0, sizeof(payload));
  return written;
}

bool AuthStore::verifyFirst(const char* deviceId, const char* credentialHex) {
  Header header = {};

  memset(entries_, 0, sizeof(entries_));
  ReadResult result;
  if (!read(header, entries_, kMaxDevices, result) || result != ReadResult::Valid ||
      header.count != 1 || !deviceId || strcmp(entries_[0].id, deviceId) != 0) return false;
  uint8_t credential[32] = {};
  if (!decodeCredential(credentialHex, credential)) return false;
  uint8_t input[48] = {};
  memcpy(input, entries_[0].salt, 16);
  memcpy(input + 16, credential, 32);
  uint8_t credentialHash[32] = {};
  const bool computed = mbedtls_sha256_ret(input, sizeof(input), credentialHash, 0) == 0;
  memset(input, 0, sizeof(input));
  memset(credential, 0, sizeof(credential));
  return computed && constantEqual(credentialHash, entries_[0].hash, 32);
}

bool AuthStore::verifyDevice(const char* deviceId, const char* credentialHex) {
  if (!deviceId || strnlen(deviceId, 9) != 7) return false;
  Header header = {};

  ReadResult result;
  if (!read(header, entries_, kMaxDevices, result) || result != ReadResult::Valid) return false;
  uint8_t credential[32] = {};
  if (!decodeCredential(credentialHex, credential)) return false;
  uint8_t candidate[32] = {};
  uint8_t input[48] = {};
  bool matched = false;
  for (size_t i = 0; i < header.count; ++i) {
    memcpy(input, entries_[i].salt, 16);
    memcpy(input + 16, credential, 32);
    if (mbedtls_sha256_ret(input, sizeof(input), candidate, 0) != 0) break;
    matched |= strcmp(entries_[i].id, deviceId) == 0 && constantEqual(candidate, entries_[i].hash, 32);
  }
  memset(input, 0, sizeof(input));
  memset(candidate, 0, sizeof(candidate));
  memset(credential, 0, sizeof(credential));
  return matched;
}

bool AuthStore::findDeviceByCredential(const char* credentialHex, char (&deviceId)[9]) {
  deviceId[0] = '\0';
  Header header = {};
  ReadResult result;
  if (!read(header, entries_, kMaxDevices, result) || result != ReadResult::Valid) return false;
  uint8_t credential[32] = {};
  if (!decodeCredential(credentialHex, credential)) return false;
  uint8_t input[48] = {};
  uint8_t candidate[32] = {};
  size_t matches = 0;
  char matchedId[9] = {};
  for (size_t i = 0; i < header.count; ++i) {
    memcpy(input, entries_[i].salt, 16);
    memcpy(input + 16, credential, 32);
    if (mbedtls_sha256_ret(input, sizeof(input), candidate, 0) != 0) {
      matches = 0;
      break;
    }
    if (constantEqual(candidate, entries_[i].hash, sizeof(candidate))) {
      ++matches;
      memcpy(matchedId, entries_[i].id, sizeof(matchedId));
    }
  }
  memset(input, 0, sizeof(input));
  memset(candidate, 0, sizeof(candidate));
  memset(credential, 0, sizeof(credential));
  if (matches != 1) {
    memset(matchedId, 0, sizeof(matchedId));
    return false;
  }
  memcpy(deviceId, matchedId, sizeof(matchedId));
  memset(matchedId, 0, sizeof(matchedId));
  return true;
}

bool AuthStore::addDevice(const char* deviceId, const char* credentialHex) {
  if (!deviceId || strnlen(deviceId, 9) != 7 || deviceId[0] != 'D' || !radioReady()) return false;
  for (size_t i = 1; i < 7; ++i) if (deviceId[i] < '0' || deviceId[i] > '9') return false;
  uint8_t credential[32] = {};
  if (!decodeCredential(credentialHex, credential)) return false;
  Header header = {};

  ReadResult result;
  if (!read(header, entries_, kMaxDevices, result) || result != ReadResult::Valid ||
      header.count >= kMaxDevices) { memset(credential, 0, sizeof(credential)); return false; }
  for (size_t i = 0; i < header.count; ++i) {
    if (strcmp(entries_[i].id, deviceId) == 0) { memset(credential, 0, sizeof(credential)); return false; }
  }
  // A browser credential may belong to only one device record, even though
  // every stored verifier uses its own salt.
  uint8_t probeInput[48] = {};
  uint8_t probeHash[32] = {};
  for (size_t i = 0; i < header.count; ++i) {
    memcpy(probeInput, entries_[i].salt, 16);
    memcpy(probeInput + 16, credential, 32);
    if (mbedtls_sha256_ret(probeInput, sizeof(probeInput), probeHash, 0) != 0) {
      memset(credential, 0, sizeof(credential));
      memset(probeInput, 0, sizeof(probeInput));
      return false;
    }
    if (constantEqual(probeHash, entries_[i].hash, sizeof(probeHash))) {
      memset(credential, 0, sizeof(credential));
      memset(probeInput, 0, sizeof(probeInput));
      memset(probeHash, 0, sizeof(probeHash));
      return false;
    }
  }
  memset(probeInput, 0, sizeof(probeInput));
  memset(probeHash, 0, sizeof(probeHash));
  DeviceAuth& entry = entries_[header.count];
  memset(&entry, 0, sizeof(entry));
  memcpy(entry.id, deviceId, 8);
  esp_fill_random(entry.salt, sizeof(entry.salt));
  uint8_t input[48] = {};
  memcpy(input, entry.salt, 16);
  memcpy(input + 16, credential, 32);
  const bool hashed = mbedtls_sha256_ret(input, sizeof(input), entry.hash, 0) == 0;
  memset(input, 0, sizeof(input));
  memset(credential, 0, sizeof(credential));
  if (!hashed) return false;
  ++header.count;
  const size_t length = sizeof(header) + header.count * sizeof(DeviceAuth);
  uint8_t* payload = scratch;
  memcpy(payload, &header, sizeof(header));
  memcpy(payload + sizeof(header), entries_, header.count * sizeof(DeviceAuth));
  const bool written = AtomicFileStore::write("/smartlock/db/auth.rec", payload, length);
  memset(payload, 0, length);
  return written;
}
