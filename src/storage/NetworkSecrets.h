#pragma once

#include <Arduino.h>
#include <Preferences.h>

class NetworkSecrets {
 public:
  enum class Result { Valid, Missing, Invalid };
  Result load(char (&password)[33]);
  bool saveSetupPassword(const char* password);

 private:
  struct Record {
    uint32_t magic;
    uint32_t schema;
    char apPassword[33];
    uint8_t reserved[3];
    uint32_t crc32;
  };
  static bool validPassword(const char* password);
  static uint32_t crc32(const uint8_t* bytes, size_t length);
};
