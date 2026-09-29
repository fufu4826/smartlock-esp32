#include <cstring>

#include "CanonicalOrigin.h"
#include "StaSecrets.h"
#include "network_manager_test_support.h"

void WiFiMock::reset() {
  persistentValue = true;
  sleepEnabled = true;
  currentMode = 0;
  currentStatus = WL_DISCONNECTED;
  apAddress = IPAddress();
  gateway = IPAddress();
  netmask = IPAddress();
  stationAddress = IPAddress();
  apActive = false;
  softApShouldFail = false;
  apStationCount = 0;
  activeApSsid[0] = '\0';
  activeApPassword[0] = '\0';
  softApCalls = 0;
  disconnectCalls = 0;
  beginCalls = 0;
  lastBeginSsid[0] = '\0';
  lastBeginPassword[0] = '\0';
}

void resetFakes() {
  WiFi.reset();
  fakeSecrets.savedSsid = "known-good-router";
  fakeSecrets.savedPassword = "known-good-password";
  fakeSecrets.saveCalls = 0;
  fakeSecrets.loadCalls = 0;
  fakeSecrets.loadValid = false;
  fakeSecrets.saveShouldFail = false;
}

const char* CanonicalOrigin::label() { return "smart-lock-test"; }
const char* CanonicalOrigin::host() { return "smart-lock-test.local"; }
bool CanonicalOrigin::begin() { return true; }
bool CanonicalOrigin::ready() { return true; }

StaSecrets::Result StaSecrets::load(char (&ssid)[33], char (&password)[65]) {
  ++fakeSecrets.loadCalls;
  std::memset(ssid, 0, 33);
  std::memset(password, 0, 65);
  if (!fakeSecrets.loadValid) return Result::Missing;
  std::memcpy(ssid, fakeSecrets.savedSsid.c_str(), fakeSecrets.savedSsid.size() + 1);
  std::memcpy(password, fakeSecrets.savedPassword.c_str(),
              fakeSecrets.savedPassword.size() + 1);
  return Result::Valid;
}

bool StaSecrets::saveConfirmed(const char* ssid, const char* password) {
  ++fakeSecrets.saveCalls;
  if (fakeSecrets.saveShouldFail) return false;
  if (!validInput(ssid, password)) return false;
  fakeSecrets.savedSsid = ssid;
  fakeSecrets.savedPassword = password;
  fakeSecrets.loadValid = true;
  return true;
}

bool StaSecrets::validInput(const char* ssid, const char* password) {
  if (!ssid || !password) return false;
  const size_t ssidLength = std::strlen(ssid);
  const size_t passwordLength = std::strlen(password);
  return ssidLength > 0 && ssidLength <= 32 && passwordLength <= 63 &&
         (passwordLength == 0 || passwordLength >= 8);
}
