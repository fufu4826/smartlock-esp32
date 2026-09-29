#include "ConfigStore.h"
#include "../security/RequestPolicy.h"

#include <stddef.h>
#include <string.h>

uint32_t ConfigStore::crc32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
  }
  return ~crc;
}

bool ConfigStore::structurallyValid(const SmartLockConfig& v) {
  if (v.schemaVersion != 1 || v.generation == 0 ||
      v.unlockDurationMs < 1000 || v.unlockDurationMs > 60000 ||
      v.accessQrLifetimeMs < 5000 || v.accessQrLifetimeMs > 300000 ||
      v.managementQrLifetimeMs < 5000 || v.managementQrLifetimeMs > 300000 ||
      v.screenTimeoutMs < 5000 || v.screenTimeoutMs > 300000 ||
      v.configured > 1 || v.ownerExists > 1 ||
      (v.configured && !v.ownerExists) || v.networkMode > 2 ||
      !RequestPolicy::utf8Text(v.deviceName, sizeof(v.deviceName))) return false;
  return crc32(reinterpret_cast<const uint8_t*>(&v), offsetof(SmartLockConfig, crc32)) == v.crc32;
}

bool ConfigStore::readSlot(const char* key, SmartLockConfig& value, bool& present) {
  present = nvs_.isKey(key);
  if (!present) return false;
  if (nvs_.getBytesLength(key) != sizeof(value)) return false;
  return nvs_.getBytes(key, &value, sizeof(value)) == sizeof(value);
}

bool ConfigStore::writeSlot(const char* key, const SmartLockConfig& value) {
  if (nvs_.putBytes(key, &value, sizeof(value)) != sizeof(value)) return false;
  SmartLockConfig readback = {};
  bool present = false;
  return readSlot(key, readback, present) && present &&
         memcmp(&readback, &value, sizeof(value)) == 0;
}

ConfigStore::Result ConfigStore::begin() {
  healthy_ = false;
  // Never resurrect stale legacy authority from an unsupported retired store.
  // This guard performs no restore, migration, selector update or erasure.
  Preferences retired;
  if(!retired.begin("sl-generation",false))return Result::NvsError;
  const bool unsupported=retired.isKey("active")||retired.isKey("txn");
  retired.end();if(unsupported)return Result::Corrupt;
  if (!nvs_.begin("sl-config", false)) return Result::NvsError;
  SmartLockConfig a = {}, b = {};
  bool hasA = false, hasB = false;
  const bool readA = readSlot("cfg-a", a, hasA);
  const bool readB = readSlot("cfg-b", b, hasB);
  const bool validA = readA && structurallyValid(a);
  const bool validB = readB && structurallyValid(b);
  // A corrupt slot can be the newer state. Falling back to an older valid
  // configuration could undo a security-sensitive change such as revocation.
  if ((hasA && !validA) || (hasB && !validB)) {
    if ((readA && a.schemaVersion > 1) || (readB && b.schemaVersion > 1))
      return Result::UnsupportedSchema;
    return Result::Corrupt;
  }
  if (validA || validB) {
    // Sequence wrap is not expected in this small configuration store.
    activeA_ = validA && (!validB || a.generation >= b.generation);
    config_ = activeA_ ? a : b;
    healthy_ = true;
    return Result::Ok;
  }
  SmartLockConfig defaults = {};
  defaults.schemaVersion = 1;
  defaults.generation = 1;
  defaults.unlockDurationMs = 10000;
  defaults.accessQrLifetimeMs = 30000;
  defaults.managementQrLifetimeMs = 90000;
  defaults.screenTimeoutMs = 30000;
  defaults.networkMode = 0;
  memcpy(defaults.deviceName, "SmartLock", 10);
  defaults.crc32 = crc32(reinterpret_cast<const uint8_t*>(&defaults), offsetof(SmartLockConfig, crc32));
  if (!writeSlot("cfg-a", defaults)) return Result::NvsError;
  config_ = defaults;
  activeA_ = true;
  healthy_ = true;
  return Result::CreatedDefaults;
}

bool ConfigStore::save(const SmartLockConfig& candidate) {
  if (!healthy_ || config_.generation == UINT32_MAX) return false;
  SmartLockConfig next = candidate;
  next.schemaVersion = 1;
  next.generation = config_.generation + 1;
  memset(next.reserved, 0, sizeof(next.reserved));
  next.crc32 = crc32(reinterpret_cast<const uint8_t*>(&next), offsetof(SmartLockConfig, crc32));
  if (!structurallyValid(next)) return false;
  const char* inactive = activeA_ ? "cfg-b" : "cfg-a";
  if (!writeSlot(inactive, next)) return false;
  config_ = next;
  activeA_ = !activeA_;
  return true;
}
