#pragma once

#include <Arduino.h>

class AtomicFileStore {
 public:
  static constexpr size_t kMaxPayload = 8192;
  enum class ReadResult { CurrentValid, BackupOnly, Missing, Corrupt };

  // Reads only a valid committed current file. Backup is never used for auth.
  static ReadResult read(const char* path, uint8_t* output, size_t capacity,
                         size_t& length);
  static bool write(const char* path, const uint8_t* payload, size_t length);
  // Only call before configuration has ever committed. Restores an interrupted
  // first-setup write; the result still requires normal logical validation.
  static bool repairUnconfiguredSetupFile(const char* path);

 private:
  struct Header {
    char magic[4];
    uint16_t schema;
    uint16_t length;
    uint32_t crc32;
  };
  static uint32_t crc32(const uint8_t* bytes, size_t length);
  static bool readOne(const char* path, uint8_t* output, size_t capacity,
                      size_t& length);
};
