#pragma once

namespace smartlock { namespace storage {
// Non-secret ID for one configured installation epoch. The hardware-derived
// unit ID remains separate and is never used as the installation identifier.
class InstallationMetadata {
 public:
  static bool begin(bool configured);
  static const char* id();
  static const char* unitId();
  static bool healthy();
  static bool clearForFactoryReset();
};
} }
