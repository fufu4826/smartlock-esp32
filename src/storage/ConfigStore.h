#pragma once

#include <Arduino.h>
#include <Preferences.h>

struct SmartLockConfig {
  uint32_t schemaVersion;
  uint32_t generation;
  uint32_t unlockDurationMs;
  uint32_t accessQrLifetimeMs;
  uint32_t managementQrLifetimeMs;
  uint32_t screenTimeoutMs;
  uint8_t configured;
  uint8_t ownerExists;
  uint8_t networkMode;
  char deviceName[25];
  uint8_t reserved[4];
  uint32_t crc32;
};

class ConfigStore {
 public:
  enum class Result { Ok, CreatedDefaults, Corrupt, UnsupportedSchema, NvsError };
  Result begin();
  const SmartLockConfig& config() const { return config_; }
  bool save(const SmartLockConfig& candidate);
  bool healthy() const { return healthy_; }

 private:
  static uint32_t crc32(const uint8_t* data, size_t length);
  static bool structurallyValid(const SmartLockConfig& value);
  bool readSlot(const char* key, SmartLockConfig& value, bool& present);
  bool writeSlot(const char* key, const SmartLockConfig& value);
  Preferences nvs_;
  SmartLockConfig config_ = {};
  bool healthy_ = false;
  bool activeA_ = true;
};
