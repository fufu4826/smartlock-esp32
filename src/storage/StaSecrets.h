#pragma once
#include <Arduino.h>
#include <Preferences.h>

class StaSecrets {
 public:
  enum class Result { Valid, Missing, Invalid };
  Result load(char (&ssid)[33], char (&password)[65]);
  bool saveConfirmed(const char* ssid, const char* password);
  static bool validInput(const char* ssid, const char* password);
 private:
  struct Record {
    uint32_t magic;
    uint32_t schema;
    uint32_t sequence;
    char ssid[33];
    char password[65];
    uint32_t crc32;
  };
  static uint32_t crc32(const uint8_t* bytes, size_t length);
  static bool readSlot(Preferences& nvs, const char* key, Record& record);
};
