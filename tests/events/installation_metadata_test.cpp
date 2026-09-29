#include "InstallationMetadata.h"
#include "Preferences.h"
#include "Arduino.h"
#include <cassert>
#include <cstring>
#include <string>

SerialClass Serial;
ESPClass ESP;
void esp_fill_random(void* output, size_t length) {
  static uint8_t seed = 1;
  auto* bytes = static_cast<uint8_t*>(output);
  for (size_t i = 0; i < length; ++i) bytes[i] = seed++;
}

int main() {
  using smartlock::storage::InstallationMetadata;
  Preferences::clearAll();
  assert(InstallationMetadata::begin(false));
  assert(InstallationMetadata::healthy() && std::strlen(InstallationMetadata::id()) == 0);
  assert(std::strlen(InstallationMetadata::unitId()) == 16);
  assert(InstallationMetadata::begin(true));
  const std::string first = InstallationMetadata::id();
  assert(first.size() == 32 && first != std::string(32, '0'));
  assert(InstallationMetadata::begin(true));
  assert(first == InstallationMetadata::id());
  assert(InstallationMetadata::clearForFactoryReset());
  assert(InstallationMetadata::begin(true));
  assert(first != InstallationMetadata::id());
  return 0;
}
