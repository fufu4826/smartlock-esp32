#include "CanonicalOrigin.h"
#include <Arduino.h>
#include <stdio.h>
#include <ESPmDNS.h>

namespace {
constexpr uint32_t kAddressSettleMs = 1500;
constexpr uint32_t kRetryIntervalMs = 5000;
char hostname[32] = {};
char fqdn[38] = {};
bool started = false;
bool addressSnapshotReady = false;
uint32_t lastStaIp = 0;
uint32_t lastApIp = 0;
uint32_t addressChangedAtMs = 0;
uint32_t lastAttemptAtMs = 0;
bool rebindPending = false;
bool retryBackoffActive = false;

void initializeName() {
  if (hostname[0]) return;
  // Full 48-bit hardware identity avoids collisions from a four-hex suffix.
  snprintf(hostname, sizeof(hostname), "smartlock-%012llx",
           (unsigned long long)(ESP.getEfuseMac() & 0xffffffffffffULL));
  snprintf(fqdn, sizeof(fqdn), "%s.local", hostname);
}

bool hasAddress(uint32_t staIp, uint32_t apIp) {
  return staIp != 0 || apIp != 0;
}

bool startMdns() {
  if (!MDNS.begin(CanonicalOrigin::label())) {
    MDNS.end();
    started = false;
    return false;
  }
  if (!MDNS.addService("http", "tcp", 80)) {
    MDNS.end();
    started = false;
    return false;
  }
  started = true;
  return true;
}

}

const char* CanonicalOrigin::label() { initializeName(); return hostname; }
const char* CanonicalOrigin::host() { initializeName(); return fqdn; }
bool CanonicalOrigin::begin() {
  if (started) return true;
  // ESPmDNS delegates address announcements to ESP-IDF.
  lastAttemptAtMs = millis();
  const bool success = startMdns();
  retryBackoffActive = !success;
  return success;
}

void CanonicalOrigin::update(uint32_t nowMs, IPAddress staIp, IPAddress apIp) {
  const uint32_t sta = static_cast<uint32_t>(staIp);
  const uint32_t ap = static_cast<uint32_t>(apIp);
  if (!addressSnapshotReady) {
    addressSnapshotReady = true;
    lastStaIp = sta;
    lastApIp = ap;
    addressChangedAtMs = nowMs;
  } else if (sta != lastStaIp || ap != lastApIp) {
    lastStaIp = sta;
    lastApIp = ap;
    addressChangedAtMs = nowMs;
    rebindPending = true;
  }

  if (rebindPending &&
      static_cast<uint32_t>(nowMs - addressChangedAtMs) >= kAddressSettleMs) {
    // IDF already owns interface announcements. Mutating TXT during a TLS
    // request allocates unrelated live state and needlessly fragments heap.
    if (started) { rebindPending = false; retryBackoffActive = false; return; }
    if (!hasAddress(sta, ap)) return;
    if (retryBackoffActive &&
        static_cast<uint32_t>(nowMs - lastAttemptAtMs) < kRetryIntervalMs) return;
    lastAttemptAtMs = nowMs;
    const bool refreshed = startMdns();
    retryBackoffActive = !refreshed;
    if (refreshed) rebindPending = false;
    return;
  }

  if (started || rebindPending || !hasAddress(sta, ap)) return;
  if (retryBackoffActive &&
      static_cast<uint32_t>(nowMs - lastAttemptAtMs) < kRetryIntervalMs) return;

  lastAttemptAtMs = nowMs;
  retryBackoffActive = !startMdns();
}

bool CanonicalOrigin::ready() { return started; }
