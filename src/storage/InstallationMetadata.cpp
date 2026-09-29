#include "InstallationMetadata.h"

#include <Preferences.h>
#include <Arduino.h>
#include <esp_system.h>
#include <string.h>

namespace {
char installationId[33] = {};
char hardwareUnitId[17] = {};
bool metadataHealthy = false;
bool validInstallationId(const char* value) {
  if (!value || strnlen(value, 33) != 32) return false;
  bool nonzero = false;
  for (size_t i = 0; i < 32; ++i) {
    const char c = value[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    nonzero |= c != '0';
  }
  return nonzero;
}
}

bool smartlock::storage::InstallationMetadata::begin(bool configured) {
  installationId[0] = '\0';
  hardwareUnitId[0] = '\0';
  metadataHealthy = false;
  snprintf(hardwareUnitId, sizeof(hardwareUnitId), "%016llx",
           static_cast<unsigned long long>(ESP.getEfuseMac()));
  if (!configured) { metadataHealthy = true; return true; }
  Preferences nvs;
  if (!nvs.begin("sl-install", false)) return false;
  char stored[33] = {};
  const size_t size = nvs.getBytesLength("id");
  bool ok = size == 32 && nvs.getBytes("id", stored, 32) == 32;
  if (ok) {
    stored[32] = '\0';
    ok = validInstallationId(stored);
  } else if (size == 0) {
    uint8_t random[16] = {};
    esp_fill_random(random, sizeof(random));
    bool nonzero = false;
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < sizeof(random); ++i) {
      nonzero |= random[i] != 0;
      stored[i * 2] = hex[random[i] >> 4];
      stored[i * 2 + 1] = hex[random[i] & 15];
    }
    stored[32] = '\0';
    ok = nonzero && nvs.putBytes("id", stored, 32) == 32;
  }
  nvs.end();
  if (!ok) return false;
  memcpy(installationId, stored, sizeof(installationId));
  metadataHealthy = true;
  return true;
}

const char* smartlock::storage::InstallationMetadata::id() { return installationId; }
const char* smartlock::storage::InstallationMetadata::unitId() { return hardwareUnitId; }
bool smartlock::storage::InstallationMetadata::healthy() { return metadataHealthy; }
bool smartlock::storage::InstallationMetadata::clearForFactoryReset() {
  Preferences nvs;
  if (!nvs.begin("sl-install", false)) return false;
  const bool ok = nvs.clear();
  nvs.end();
  if (ok) { installationId[0] = '\0'; metadataHealthy = false; }
  return ok;
}
