#include <cstdlib>
#include <iostream>
#include <string>

#include "NetworkManager.h"
#include "CanonicalOrigin.h"
#include "network_manager_test_support.h"

SerialMock Serial;
EspMock ESP;
WiFiMock WiFi;
FakeSecretsState fakeSecrets;

namespace {
int checks = 0;

void check(bool condition, const char* description) {
  ++checks;
  if (!condition) {
    std::cerr << "FAIL: " << description << '\n';
    std::exit(1);
  }
}

void setupConfiguredAp(NetworkManager& manager) {
  check(manager.beginConfiguredAp("0123456789abcdef0123456789abcdef"),
        "configured AP starts");
  check(manager.ready(), "configured AP reports ready");
  check(WiFi.apActive, "AP remains active");
  check(!WiFi.persistentValue, "SDK credential persistence is disabled");
  check(!WiFi.sleepEnabled, "modem sleep is disabled for local availability");
}

void oldConnectedCandidateWaitsForDisconnect() {
  resetFakes();
  NetworkManager manager;
  setupConfiguredAp(manager);
  WiFi.currentStatus = WL_CONNECTED;
  WiFi.stationAddress = IPAddress(10, 0, 0, 20);

  check(manager.startCandidateSta("new-router", "candidate-pass", 100),
        "candidate transaction starts");
  manager.update(101);
  check(WiFi.beginCalls == 0,
        "candidate association waits while stale old connection is reported");
  check(fakeSecrets.saveCalls == 0,
        "stale old connection does not persist candidate credentials");

  WiFi.currentStatus = WL_DISCONNECTED;
  manager.update(102);
  check(WiFi.beginCalls == 1, "candidate starts after disconnect is observed");
  check(fakeSecrets.saveCalls == 0, "association start alone does not save");

  WiFi.currentStatus = WL_DISCONNECTED;
  manager.update(20102);
  check(manager.staState() == NetworkManager::StaState::Failed,
        "failed candidate reaches failed state after timeout");
  check(fakeSecrets.saveCalls == 0, "failed candidate is never saved");
  check(fakeSecrets.savedSsid == "known-good-router" &&
            fakeSecrets.savedPassword == "known-good-password",
        "failed candidate retains previously saved configuration");
  check(WiFi.apActive && WiFi.softAPIP() == IPAddress(192, 168, 4, 1),
        "failed candidate leaves configured AP available");
}

void onlyConnectedNonzeroIpIsPersisted() {
  resetFakes();
  NetworkManager manager;
  setupConfiguredAp(manager);
  check(manager.startCandidateSta("confirmed-router", "confirmed-pass", 5),
        "candidate transaction starts from disconnected state");
  manager.update(6);
  check(WiFi.beginCalls == 1, "candidate association begins");
  check(fakeSecrets.saveCalls == 0, "association begin does not save");

  WiFi.currentStatus = WL_CONNECTED;
  WiFi.stationAddress = IPAddress(0, 0, 0, 0);
  manager.update(7);
  check(fakeSecrets.saveCalls == 0,
        "connected status without an assigned IP does not save");

  WiFi.stationAddress = IPAddress(192, 0, 2, 44);
  manager.update(8);
  check(manager.staState() == NetworkManager::StaState::Connected,
        "connected candidate reaches connected state after IP assignment");
  check(fakeSecrets.saveCalls == 1,
        "candidate credentials save once after successful nonzero IP");
  check(fakeSecrets.savedSsid == "confirmed-router" &&
            fakeSecrets.savedPassword == "confirmed-pass",
        "successful candidate replaces the simulated saved configuration");
}

void disconnectTimeoutDoesNotStartCandidate() {
  resetFakes();
  NetworkManager manager;
  setupConfiguredAp(manager);
  WiFi.currentStatus = WL_CONNECTED;
  WiFi.stationAddress = IPAddress(10, 0, 0, 20);
  check(manager.startCandidateSta("new-router", "candidate-pass", 100),
        "candidate starts while prior association is connected");

  manager.update(20100);
  check(manager.staState() == NetworkManager::StaState::Failed,
        "disconnect timeout reaches failed state");
  check(WiFi.beginCalls == 0, "disconnect timeout never starts candidate association");
  check(fakeSecrets.saveCalls == 0, "disconnect timeout does not save candidate");
  check(fakeSecrets.savedSsid == "known-good-router" &&
            fakeSecrets.savedPassword == "known-good-password",
        "disconnect timeout retains saved configuration");
  check(WiFi.apActive && WiFi.softAPIP() == IPAddress(192, 168, 4, 1),
        "disconnect timeout leaves configured AP available");
}

void storageFailureDoesNotReportConnectedOrReplaceSavedConfig() {
  resetFakes();
  NetworkManager manager;
  setupConfiguredAp(manager);
  check(manager.startCandidateSta("candidate-router", "candidate-pass", 30),
        "candidate starts for storage-failure case");
  manager.update(31);
  WiFi.currentStatus = WL_CONNECTED;
  WiFi.stationAddress = IPAddress(203, 0, 113, 10);
  fakeSecrets.saveShouldFail = true;
  manager.update(32);

  check(manager.staState() == NetworkManager::StaState::Failed,
        "storage failure does not report candidate as connected");
  check(fakeSecrets.saveCalls == 1, "confirmed connection attempts one save");
  check(fakeSecrets.savedSsid == "known-good-router" &&
            fakeSecrets.savedPassword == "known-good-password",
        "failed save retains previous configuration");
  check(WiFi.currentStatus == WL_CONNECTED && WiFi.disconnectCalls >= 2,
        "storage failure requests station disconnect while retaining AP");
  check(WiFi.apActive && WiFi.softAPIP() == IPAddress(192, 168, 4, 1),
        "storage failure leaves configured AP available");
}

void savedBootConnectionDoesNotResave() {
  resetFakes();
  fakeSecrets.savedSsid = "boot-router";
  fakeSecrets.savedPassword = "boot-password";
  fakeSecrets.loadValid = true;
  NetworkManager manager;
  setupConfiguredAp(manager);

  check(manager.startSavedSta(20), "saved STA starts");
  check(fakeSecrets.loadCalls == 1, "saved STA is loaded once");
  manager.update(21);
  check(WiFi.beginCalls == 1, "saved STA association begins");
  WiFi.currentStatus = WL_CONNECTED;
  WiFi.stationAddress = IPAddress(198, 51, 100, 9);
  manager.update(22);
  check(manager.staState() == NetworkManager::StaState::Connected,
        "saved STA reaches connected state");
  check(fakeSecrets.saveCalls == 0, "saved boot connection is not written again");
  check(fakeSecrets.savedSsid == "boot-router" &&
            fakeSecrets.savedPassword == "boot-password",
        "saved boot configuration remains unchanged");
}

void outageEntersProtectedRecoveryAtSixtySecondsAndRetriesSavedNetwork() {
  resetFakes();
  NetworkManager manager;
  setupConfiguredAp(manager);
  check(manager.startCandidateSta("known-good-router", "known-good-password", 10),
        "known-good candidate starts");
  manager.update(11);
  WiFi.currentStatus = WL_CONNECTED;
  WiFi.stationAddress = IPAddress(192, 0, 2, 77);
  manager.update(12);
  check(manager.staState() == NetworkManager::StaState::Connected,
        "known-good candidate connects before outage");
  check(fakeSecrets.saveCalls == 1, "known-good candidate is saved once");

  WiFi.currentStatus = WL_DISCONNECTED;
  WiFi.stationAddress = IPAddress(0, 0, 0, 0);
  manager.update(100);
  check(manager.staState() == NetworkManager::StaState::Failed,
        "lost known-good link enters failed state");
  check(!manager.recoveryActive(), "recovery is inactive before sixty seconds");
  check(std::string(manager.ssid()).find("SmartLock-") == 0,
        "normal AP name remains before recovery threshold");
  check(fakeSecrets.loadCalls == 0 && WiFi.beginCalls == 1,
        "known-good retry does not happen early");

  manager.update(60099);
  check(!manager.recoveryActive(), "recovery remains inactive at 59,999 ms");
  check(fakeSecrets.loadCalls == 0 && WiFi.beginCalls == 1,
        "saved network is not retried before sixty seconds");

  manager.update(60100);
  check(manager.recoveryActive(), "recovery activates at sixty seconds");
  check(std::string(manager.ssid()).find("SmartLock-Recovery-") == 0,
        "recovery AP uses the recovery SSID prefix");
  check(std::string(WiFi.activeApSsid).find("SmartLock-Recovery-") == 0,
        "recovery SSID is applied to WiFi AP");
  check(std::string(WiFi.activeApPassword) == "0123456789abcdef0123456789abcdef",
        "recovery AP retains its configured password");
  check(fakeSecrets.loadCalls == 1 && WiFi.beginCalls == 2,
        "recovery retries the known-good saved network");
  check(std::string(WiFi.lastBeginSsid) == "known-good-router" &&
            std::string(WiFi.lastBeginPassword) == "known-good-password",
        "retry uses saved known-good credentials");

  WiFi.currentStatus = WL_CONNECTED;
  WiFi.stationAddress = IPAddress(198, 51, 100, 8);
  manager.update(60101);
  check(manager.staState() == NetworkManager::StaState::Connected,
        "saved network retry can restore the station link");
  check(fakeSecrets.saveCalls == 1,
        "saved-network recovery does not rewrite confirmed credentials");
  check(WiFi.apActive && manager.recoveryActive(),
        "protected recovery AP remains available after station recovery");
}

void recoveryWithoutKnownGoodDoesNotWriteConfiguration() {
  resetFakes();
  fakeSecrets.savedSsid.clear();
  fakeSecrets.savedPassword.clear();
  fakeSecrets.loadValid = false;
  NetworkManager manager;
  setupConfiguredAp(manager);
  check(manager.startCandidateSta("unconfirmed-router", "unconfirmed-password", 1),
        "unconfirmed candidate starts");
  manager.update(2);
  WiFi.currentStatus = WL_DISCONNECTED;
  manager.update(20002);
  check(manager.staState() == NetworkManager::StaState::Failed,
        "unconfirmed candidate fails without saving");
  manager.update(20003);
  check(!manager.recoveryActive(), "missing-known-good case waits before recovery");
  manager.update(80003);

  check(manager.recoveryActive(), "recovery AP activates without known-good STA data");
  check(std::string(WiFi.activeApSsid).find("SmartLock-Recovery-") == 0,
        "recovery AP remains available when no saved station config exists");
  check(fakeSecrets.loadCalls == 1, "missing-known-good case checks storage once");
  check(WiFi.beginCalls == 1, "missing-known-good case does not start a blank retry");
  check(fakeSecrets.saveCalls == 0 && fakeSecrets.savedSsid.empty() &&
            fakeSecrets.savedPassword.empty(),
        "missing-known-good recovery does not overwrite station configuration");
  check(WiFi.apActive &&
            std::string(WiFi.activeApPassword) == "0123456789abcdef0123456789abcdef",
        "recovery without saved STA data keeps the protected AP available");
}

void explicitLocalFallbackPinsRecoveryAcrossLanDiagnostics() {
  resetFakes();
  NetworkManager manager;
  setupConfiguredAp(manager);
  check(manager.startCandidateSta("known-good-router", "known-good-password", 10),
        "candidate starts before explicit fallback");
  manager.update(11);
  WiFi.currentStatus = WL_CONNECTED;
  WiFi.stationAddress = IPAddress(192, 0, 2, 88);
  manager.update(12);
  const unsigned staBeginCalls = WiFi.beginCalls;
  const unsigned staDisconnectCalls = WiFi.disconnectCalls;
  const IPAddress stationIp = manager.staIp();

  check(manager.requestLocalFallback(100), "explicit local fallback request succeeds");
  check(manager.recoveryActive() && manager.apEnabled(),
        "explicit request exposes the existing protected recovery AP");
  check(manager.staState() == NetworkManager::StaState::Connected &&
            manager.staIp() == stationIp && WiFi.beginCalls == staBeginCalls &&
            WiFi.disconnectCalls == staDisconnectCalls,
        "fallback request leaves the connected STA association untouched");

  char payload[128] = {};
  check(manager.buildFallbackWifiQr(payload, sizeof(payload)),
        "generated recovery AP can produce a Wi-Fi QR payload");
  check(std::string(payload).find("WIFI:T:WPA;S:SmartLock-Recovery-") == 0 &&
            std::string(payload).find(";P:0123456789abcdef0123456789abcdef;;") != std::string::npos,
        "Wi-Fi QR contains only the synthetic recovery SSID and protected AP password");
  const unsigned apCalls = WiFi.softApCalls;
  check(manager.requestLocalFallback(200), "repeat local fallback request renews the lease");
  check(WiFi.softApCalls == apCalls,
        "renewal does not restart an already active recovery AP");
  manager.noteLanRequest(300);
  manager.update(3300);
  check(manager.apEnabled() && WiFi.apActive && manager.staIp() == stationIp,
        "LAN diagnostics and grace expiry cannot close a pinned fallback AP");

  char shortPayload[8] = "dirty";
  check(!manager.buildFallbackWifiQr(shortPayload, sizeof(shortPayload)) &&
            shortPayload[0] == '\0',
        "undersized QR output is rejected and cleared without a partial secret");
}

void localFallbackLeaseIsWrapSafeAndExpires() {
  resetFakes();
  NetworkManager manager;
  setupConfiguredAp(manager);
  check(manager.startCandidateSta("home", "home-password", 1),
        "candidate starts before wraparound lease test");
  manager.update(2);
  WiFi.currentStatus = WL_CONNECTED;
  WiFi.stationAddress = IPAddress(198, 51, 100, 22);
  manager.update(3);
  const uint32_t leaseStart = 0xfffffff0u;
  check(manager.requestLocalFallback(leaseStart), "fallback lease starts near millis wrap");
  manager.noteLanRequest(leaseStart + 10u);
  manager.update(leaseStart + 3010u);
  check(WiFi.apActive && manager.apEnabled(),
        "wraparound time arithmetic keeps fallback AP pinned during lease");
  manager.update(leaseStart + 299999u);
  check(WiFi.apActive && manager.apEnabled(),
        "fallback remains pinned through the final millisecond of its lease");
  manager.update(leaseStart + 300000u);
  check(!manager.apEnabled() && !WiFi.apActive,
        "after lease expiry the verified LAN policy closes an unused AP");
}

void localFallbackRenewalAndConnectedClientExtendAvailability() {
  resetFakes();
  NetworkManager manager;
  setupConfiguredAp(manager);
  check(manager.startCandidateSta("home", "home-password", 1),
        "candidate starts before renewal test");
  manager.update(2);
  WiFi.currentStatus = WL_CONNECTED;
  WiFi.stationAddress = IPAddress(203, 0, 113, 21);
  manager.update(3);
  check(manager.requestLocalFallback(100), "fallback lease starts");
  manager.noteLanRequest(200);
  manager.update(3200);
  check(manager.requestLocalFallback(200000), "second QR request renews lease");
  manager.update(300100);
  check(manager.apEnabled() && WiFi.apActive,
        "renewed lease stays active past the original deadline");
  WiFi.apStationCount = 1;
  manager.update(500000);
  check(manager.apEnabled() && WiFi.apActive,
        "connected recovery-AP clients prevent closure after the renewed lease expires");
  WiFi.apStationCount = 0;
  manager.update(500001);
  check(!manager.apEnabled() && !WiFi.apActive,
        "LAN close policy resumes after expiry and the last AP client disconnects");
}

void localFallbackConnectionFailurePreservesStationAndCredentials() {
  resetFakes();
  NetworkManager manager;
  setupConfiguredAp(manager);
  check(manager.startCandidateSta("home", "home-password", 1),
        "candidate starts before AP failure test");
  manager.update(2);
  WiFi.currentStatus = WL_CONNECTED;
  WiFi.stationAddress = IPAddress(192, 0, 2, 44);
  manager.update(3);
  const IPAddress stationIp = manager.staIp();
  const unsigned begins = WiFi.beginCalls;
  const unsigned disconnects = WiFi.disconnectCalls;
  WiFi.softApShouldFail = true;
  check(!manager.requestLocalFallback(100), "failed recovery AP start is reported");
  check(!manager.recoveryActive() && manager.apEnabled() && WiFi.apActive,
        "AP failure leaves the current configured AP available");
  check(manager.staState() == NetworkManager::StaState::Connected &&
            manager.staIp() == stationIp && WiFi.beginCalls == begins &&
            WiFi.disconnectCalls == disconnects,
        "AP failure does not disturb STA or start a reconnect");
  check(fakeSecrets.savedSsid == "home" &&
            fakeSecrets.savedPassword == "home-password" && fakeSecrets.saveCalls == 1,
        "AP failure preserves the last confirmed station credentials");
}
}  // namespace

int main() {
  {
    resetFakes(); NetworkManager manager; setupConfiguredAp(manager);
    check(manager.startCandidateSta("home", "home-password", 1), "candidate starts for LAN policy");
    manager.update(2); WiFi.currentStatus = WL_CONNECTED; WiFi.stationAddress = IPAddress(192,168,1,179);
    manager.update(3); manager.update(10000);
    check(manager.apEnabled() && WiFi.apActive, "AP retained before actual LAN verification");
    manager.noteLanRequest(10001); manager.update(13000);
    check(manager.apEnabled(), "LAN verification response grace retained");
    manager.update(13001);
    check(!manager.apEnabled() && !WiFi.apActive && WiFi.currentMode == WIFI_STA, "verified LAN becomes primary with AP off");
    manager.update(14000);
    check(!WiFi.apActive, "update does not accidentally recreate AP");
    manager.beginScan(); check(WiFi.currentMode == WIFI_STA, "scan keeps STA-only mode");
    WiFi.currentStatus = WL_DISCONNECTED; manager.update(15000); manager.update(75000);
    check(manager.apEnabled() && manager.recoveryActive() && WiFi.apActive, "outage restores protected recovery after one minute");
    check(std::string(WiFi.activeApPassword) == "0123456789abcdef0123456789abcdef", "recovery keeps password after AP off");
  }
  oldConnectedCandidateWaitsForDisconnect();
  onlyConnectedNonzeroIpIsPersisted();
  disconnectTimeoutDoesNotStartCandidate();
  storageFailureDoesNotReportConnectedOrReplaceSavedConfig();
  savedBootConnectionDoesNotResave();
  outageEntersProtectedRecoveryAtSixtySecondsAndRetriesSavedNetwork();
  recoveryWithoutKnownGoodDoesNotWriteConfiguration();
  explicitLocalFallbackPinsRecoveryAcrossLanDiagnostics();
  localFallbackLeaseIsWrapSafeAndExpires();
  localFallbackRenewalAndConnectedClientExtendAvailability();
  localFallbackConnectionFailurePreservesStationAndCredentials();
  std::cout << "PASS: " << checks << " assertions across 12 isolated scenarios\n";
}
