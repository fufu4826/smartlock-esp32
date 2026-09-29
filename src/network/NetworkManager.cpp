#include "NetworkManager.h"

#include <stdio.h>
#include <string.h>

#include <WiFi.h>
#include "CanonicalOrigin.h"

namespace {
constexpr uint32_t kLocalFallbackLeaseMs = 5u * 60u * 1000u;
constexpr char kRecoverySsidPrefix[] = "SmartLock-Recovery-";

bool isGeneratedRecoverySsid(const char* ssid) {
  if (!ssid) return false;
  const size_t prefixLength = sizeof(kRecoverySsidPrefix) - 1;
  if (strnlen(ssid, 33) != prefixLength + 4 ||
      memcmp(ssid, kRecoverySsidPrefix, prefixLength) != 0) return false;
  for (size_t i = prefixLength; i < prefixLength + 4; ++i) {
    const char c = ssid[i];
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) return false;
  }
  return true;
}

bool isGeneratedApPassword(const char* password) {
  if (!password || strnlen(password, 33) != 32) return false;
  for (size_t i = 0; i < 32; ++i) {
    const char c = password[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}
}  // namespace

bool NetworkManager::beginSetupAp() {
  ready_ = false;
  // Only StaSecrets may persist a confirmed STA transaction. The SDK must
  // never save an untested candidate through WiFi.begin().
  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.setHostname(CanonicalOrigin::label());
  if (!WiFi.mode(WIFI_AP)) return false;
  const uint64_t mac = ESP.getEfuseMac();
  snprintf(ssid_, sizeof(ssid_), "SmartLock-%04X", static_cast<unsigned>(mac & 0xffff));
  const IPAddress address(192, 168, 4, 1);
  const IPAddress gateway(192, 168, 4, 1);
  const IPAddress netmask(255, 255, 255, 0);
  if (!WiFi.softAPConfig(address, gateway, netmask)) return false;
  // The first-setup AP is intentionally open. Owner creation is not enabled
  // until the authenticated Phase 5 setup flow is implemented.
  if (!WiFi.softAP(ssid_)) return false;
  WiFi.softAPsetHostname(CanonicalOrigin::label());
  ip_ = WiFi.softAPIP();
  ready_ = ip_ == address;
  return ready_;
}

bool NetworkManager::beginConfiguredAp(const char* password) {
  ready_ = false;
  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.setHostname(CanonicalOrigin::label());
  if (!password || strlen(password) != 32 || !WiFi.mode(WIFI_AP)) return false;
  memcpy(apPassword_, password, 33);
  const uint64_t mac = ESP.getEfuseMac();
  snprintf(ssid_, sizeof(ssid_), "SmartLock-%04X", static_cast<unsigned>(mac & 0xffff));
  const IPAddress address(192, 168, 4, 1);
  if (!WiFi.softAPConfig(address, address, IPAddress(255, 255, 255, 0))) return false;
  if (!WiFi.softAP(ssid_, password)) return false;
  WiFi.softAPsetHostname(CanonicalOrigin::label());
  ip_ = WiFi.softAPIP();
  ready_ = ip_ == address;
  return ready_;
}

bool NetworkManager::startSavedSta(uint32_t nowMs) {
  char ssid[33] = {}, password[65] = {};
  if (secrets_.load(ssid, password) != StaSecrets::Result::Valid) return false;
  // A configured AP is an Owner recovery endpoint, never an enrollment LAN.
  if (!enterRecovery()) { memset(password, 0, sizeof(password)); return false; }
  const bool started = startCandidateSta(ssid, password, nowMs);
  saveOnConnect_ = false;
  memset(password, 0, sizeof(password));
  return started;
}

bool NetworkManager::startCandidateSta(const char* ssid, const char* password,
                                       uint32_t nowMs) {
  if (!ready_ || !apPassword_[0] || staState_ == StaState::Connecting ||
      !StaSecrets::validInput(ssid, password)) return false;
  if (!WiFi.mode(WIFI_AP_STA)) return false;
  if (!apEnabled_ && !enterRecovery()) return false;
  lanVerified_ = false;
  if (WiFi.softAPIP() != ip_ && !WiFi.softAP(ssid_, apPassword_)) return false;
  memset(staSsid_, 0, sizeof(staSsid_));
  memset(candidatePassword_, 0, sizeof(candidatePassword_));
  memcpy(staSsid_, ssid, strlen(ssid) + 1);
  memcpy(candidatePassword_, password, strlen(password) + 1);
  // Observe the old connection becoming disconnected before starting the new
  // association. Otherwise a cached WL_CONNECTED/old IP could commit a bad
  // password for the same SSID before its connection attempt has happened.
  WiFi.disconnect(false);
  waitingForDisconnect_ = true;
  staStartedMs_ = nowMs;
  saveOnConnect_ = true;
  staState_ = StaState::Connecting;
  return true;
}

bool NetworkManager::enterRecovery() {
  // Recovery changes only the AP name. Keep the existing WPA2 password and
  // all Owner/device authorization; physical possession is not authorization.
  if (!ready_ || !apPassword_[0]) return false;
  if (!WiFi.mode(WIFI_AP_STA)) return false;
  char recoveryName[33] = {};
  snprintf(recoveryName, sizeof(recoveryName), "%s%04X", kRecoverySsidPrefix,
           static_cast<unsigned>(ESP.getEfuseMac() & 0xffff));
  if (!WiFi.softAP(recoveryName, apPassword_)) return false;
  snprintf(ssid_, sizeof(ssid_), "%s", recoveryName);
  WiFi.softAPsetHostname(CanonicalOrigin::label());
  recoveryActive_ = true;
  apEnabled_ = true;
  lanVerified_ = false;
  Serial.println("NETWORK: PROTECTED RECOVERY AP");
  return true;
}

bool NetworkManager::requestLocalFallback(uint32_t nowMs) {
  // Repeated QR requests renew the lease without restarting an AP that a
  // phone may already be using.
  if (!(recoveryActive_ && apEnabled_ && WiFi.softAPIP() == ip_) &&
      !enterRecovery()) return false;
  localFallbackStartedAt_ = nowMs;
  localFallbackPinned_ = true;
  return true;
}

bool NetworkManager::buildFallbackWifiQr(char* out, size_t capacity) const {
  if (!out || capacity == 0) return false;
  out[0] = '\0';
  if (!recoveryActive_ || !apEnabled_ || !isGeneratedRecoverySsid(ssid_) ||
      !isGeneratedApPassword(apPassword_)) return false;
  const int written = snprintf(out, capacity, "WIFI:T:WPA;S:%s;P:%s;;",
                               ssid_, apPassword_);
  if (written < 0 || static_cast<size_t>(written) >= capacity) {
    out[0] = '\0';
    return false;
  }
  return true;
}

void NetworkManager::update(uint32_t nowMs) {
  if (apEnabled_ && ready_ && apPassword_[0] && WiFi.softAPIP() != ip_)
    WiFi.softAP(ssid_, apPassword_);
  if (staState_ == StaState::Connected && WiFi.status() != WL_CONNECTED)
    staState_ = StaState::Failed;
  if (staState_ == StaState::Failed) {
    if (!failureObserved_) {
      failureObserved_ = true;
      failedAtMs_ = retryAtMs_ = nowMs;
    }
    // Preserve the normal AP for the first minute of failure. Repeated saved
    // STA retries never overwrite confirmed settings or touch authorization.
    if (static_cast<uint32_t>(nowMs - failedAtMs_) >= 60000 && !recoveryActive_)
      enterRecovery();
    if (static_cast<uint32_t>(nowMs - retryAtMs_) >= 60000) {
      retryAtMs_ = nowMs;
      startSavedSta(nowMs);
    }
  } else if (staState_ == StaState::Connected) {
    failureObserved_ = false;
    const bool fallbackLeaseActive = localFallbackPinned_ &&
        static_cast<uint32_t>(nowMs - localFallbackStartedAt_) < kLocalFallbackLeaseMs;
    if (localFallbackPinned_ && !fallbackLeaseActive) localFallbackPinned_ = false;
    // Close recovery only after a request actually arrived through STA.
    // The response gets a grace interval; a failed candidate never closes AP.
    if (apEnabled_ && lanVerified_ && !fallbackLeaseActive &&
        WiFi.softAPgetStationNum() == 0 &&
        static_cast<uint32_t>(nowMs - lanVerifiedAt_) >= 3000) {
      if (WiFi.mode(WIFI_STA)) {
        apEnabled_ = false; recoveryActive_ = false; lanVerified_ = false;
        Serial.println("NETWORK: HOME LAN PRIMARY; AP OFF");
      }
    }
  }
  if (staState_ != StaState::Connecting) return;
  if (waitingForDisconnect_) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(staSsid_, candidatePassword_);
      waitingForDisconnect_ = false;
      staStartedMs_ = nowMs;
    } else if (static_cast<uint32_t>(nowMs - staStartedMs_) >= 20000) {
      memset(candidatePassword_, 0, sizeof(candidatePassword_));
      saveOnConnect_ = false;
      waitingForDisconnect_ = false;
      staState_ = StaState::Failed;
      Serial.println("STA: FAILED (disconnect timeout)");
    }
    return;
  }
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    const bool persisted = !saveOnConnect_ || secrets_.saveConfirmed(staSsid_, candidatePassword_);
    memset(candidatePassword_, 0, sizeof(candidatePassword_));
    saveOnConnect_ = false;
    staState_ = persisted ? StaState::Connected : StaState::Failed;
    if (persisted) Serial.printf("STA: CONNECTED IP %s\n", WiFi.localIP().toString().c_str());
    else Serial.println("STA: FAILED (configuration storage)");
    if (!persisted) WiFi.disconnect(false);
  } else if (static_cast<uint32_t>(nowMs - staStartedMs_) >= 20000) {
    WiFi.disconnect(false);
    memset(candidatePassword_, 0, sizeof(candidatePassword_));
    saveOnConnect_ = false;
    staState_ = StaState::Failed;
    Serial.println("STA: FAILED (timeout)");
  }
}

IPAddress NetworkManager::staIp() const {
  return staState_ == StaState::Connected && WiFi.status() == WL_CONNECTED
             ? WiFi.localIP() : IPAddress(0, 0, 0, 0);
}

int32_t NetworkManager::staRssi() const {
  return staState_ == StaState::Connected && WiFi.status() == WL_CONNECTED
             ? WiFi.RSSI() : 0;
}

bool NetworkManager::beginScan() {
  if (!ready_ || staState_ == StaState::Connecting || !WiFi.mode(apEnabled_ ? WIFI_AP_STA : WIFI_STA)) return false;
  const int current = WiFi.scanComplete();
  if (current == WIFI_SCAN_RUNNING) return true;
  WiFi.scanDelete();
  return WiFi.scanNetworks(true, false) >= WIFI_SCAN_RUNNING;
}

int NetworkManager::scanComplete() const { return WiFi.scanComplete(); }
const char* NetworkManager::scannedSsid(int index) const {
  static String value;
  value = WiFi.SSID(index);
  return value.c_str();
}
int32_t NetworkManager::scannedRssi(int index) const { return WiFi.RSSI(index); }
bool NetworkManager::scannedSecure(int index) const {
  return WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
}
void NetworkManager::clearScan() { WiFi.scanDelete(); }

void NetworkManager::noteLanRequest(uint32_t nowMs) {
  if (apEnabled_ && !lanVerified_ && staIp() != IPAddress(0, 0, 0, 0)) {
    lanVerified_ = true; lanVerifiedAt_ = nowMs;
  }
}
