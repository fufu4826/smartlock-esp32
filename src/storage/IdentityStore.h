#pragma once
#include "RecordCodec.h"

namespace smartlock { namespace storage {
class IdentityStore {
 public:
  static constexpr size_t kMaxRecords = 64;
  // The committed mode marker and current identity file are both mandatory.
  static bool load(IdentityRecord*, size_t capacity, size_t& count);
  static bool replace(const IdentityRecord*, size_t count);
  static bool healthy();
  static bool find(const char* id, IdentityRecord& record);
  static bool normalizeName(const char* input, char (&out)[41]);
  static bool nameAvailable(const char* normalized);
  static bool modePresent();
  // One-time, explicit boot migration. Never a normal authorization fallback.
  static bool migrateLegacy();
  static bool createFirst(const char* ownerName);
  static bool backupVerifierUnchanged();
};
} }
