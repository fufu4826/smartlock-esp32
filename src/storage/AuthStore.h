#pragma once

#include <Arduino.h>

class AuthStore {
 public:
  enum class ReadResult { Valid, Missing, Invalid };
  static constexpr size_t kMaxDevices = 64;

  // Single-task calls. First setup requires a missing credential store;
  // existing authority is never overwritten.
  static bool saveFirst(const char* deviceId, const char* credentialHex);
  static ReadResult inspect(size_t& deviceCount);
  static bool hasDevice(const char* deviceId);
  static bool verifyFirst(const char* deviceId, const char* credentialHex);
  static bool verifyDevice(const char* deviceId, const char* credentialHex);
  // Find the unique stored verifier matching a credential without exposing
  // salt or hash material. Fails closed for invalid stores or ambiguous matches.
  static bool findDeviceByCredential(const char* credentialHex, char (&deviceId)[9]);
  static bool addDevice(const char* deviceId, const char* credentialHex);
  static bool ownerVerifierMatches(const char* backupPath);

 private:
  struct DeviceAuth {
    char id[9];
    uint8_t salt[16];
    uint8_t hash[32];
  };
  struct Header {
    uint32_t magic;
    uint32_t schema;
    uint32_t count;
  };
  static DeviceAuth entries_[kMaxDevices];
  static bool decodeCredential(const char* hex, uint8_t (&bytes)[32]);
  static bool read(Header& header, DeviceAuth* entries, size_t capacity,
                   ReadResult& result);
};
