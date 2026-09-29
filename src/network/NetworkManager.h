#pragma once

#include <stddef.h>

#include <Arduino.h>
#include <IPAddress.h>
#include "../storage/StaSecrets.h"

class NetworkManager {
 public:
  bool beginSetupAp();
  bool beginConfiguredAp(const char* password);
  bool startSavedSta(uint32_t nowMs);
  bool startCandidateSta(const char* ssid, const char* password, uint32_t nowMs);
  bool enterRecovery();
  bool requestLocalFallback(uint32_t nowMs);
  bool buildFallbackWifiQr(char* out, size_t capacity) const;
  bool recoveryActive() const { return recoveryActive_; }
  bool apEnabled() const { return apEnabled_; }
  void noteLanRequest(uint32_t nowMs);
  void update(uint32_t nowMs);
  enum class StaState { Disconnected, Connecting, Connected, Failed };
  StaState staState() const { return staState_; }
  const char* staSsid() const { return staSsid_; }
  IPAddress staIp() const;
  int32_t staRssi() const;
  bool beginScan();
  int scanComplete() const;
  const char* scannedSsid(int index) const;
  int32_t scannedRssi(int index) const;
  bool scannedSecure(int index) const;
  void clearScan();
  const char* ssid() const { return ssid_; }
  IPAddress ip() const { return ip_; }
  bool ready() const { return ready_; }

 private:
  char ssid_[33] = {};
  IPAddress ip_;
  bool ready_ = false;
  char apPassword_[33] = {};
  char staSsid_[33] = {};
  char candidatePassword_[65] = {};
  StaSecrets secrets_;
  StaState staState_ = StaState::Disconnected;
  uint32_t staStartedMs_ = 0;
  bool saveOnConnect_ = false;
  bool waitingForDisconnect_ = false;
  bool recoveryActive_ = false;
  bool apEnabled_ = true;
  bool lanVerified_ = false;
  uint32_t lanVerifiedAt_ = 0;
  bool localFallbackPinned_ = false;
  uint32_t localFallbackStartedAt_ = 0;
  bool failureObserved_ = false;
  uint32_t failedAtMs_ = 0;
  uint32_t retryAtMs_ = 0;
};
