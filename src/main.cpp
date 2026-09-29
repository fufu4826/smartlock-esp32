#include "storage/IdentityStore.h"
#include "storage/AuthStore.h"
#include "storage/InstallationMetadata.h"
#include <esp_heap_caps.h>
#include <Arduino.h>
#include <Preferences.h>
#include <nvs.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <SD.h>

#include "app/AppStateMachine.h"
#include "app/PhysicalAdmin.h"
#include "app/AdminTouchRouter.h"
#include "hardware/AdminDisplay.h"
#include "hardware/DisplayManager.h"
#include "hardware/LockController.h"
#include "hardware/TouchManager.h"
#include "storage/ConfigStore.h"
#include "storage/StorageHealth.h"
#include "storage/Phase2SelfTest.h"
#include "qr/SessionManager.h"
#include "qr/QrManager.h"
#include "network/NetworkManager.h"
#include "network/CanonicalOrigin.h"
#include "network/CaptivePortal.h"
#include "network/WebServerManager.h"
#include "app/FirstOwnerSetup.h"
#include "app/EnrollmentManager.h"
#include "app/AccessController.h"
#include "app/FactoryResetController.h"
#include "storage/NetworkSecrets.h"
#include "events/Diagnostics.h"
#include "events/TimeManager.h"
#include "notifications/LineNotifications.h"
#include <esp_system.h>

namespace {
constexpr int kRecalibratePin = 0;
TFT_eSPI tft;
Preferences preferences;
LockController lockController;
DisplayManager displayManager;
TouchManager touchManager;
AppStateMachine app;
PhysicalAdmin physicalAdmin;
AdminTouchRouter adminTouchRouter;
ConfigStore configStore;
StorageHealth storageHealth;
SessionManager sessions;
QrManager qr;
NetworkManager network;
CaptivePortal portal;
WebServerManager web;
NetworkSecrets networkSecrets;
FirstOwnerSetup firstOwner(configStore, storageHealth, sessions, networkSecrets);
EnrollmentManager enrollment(sessions);
AccessController accessController(sessions, lockController, configStore, enrollment);
FactoryResetController factoryReset(sessions, lockController, enrollment);
bool systemFault = false;
bool safeBootInitialized=false;
bool canonicalReady=false;
bool httpReady=false;
bool adminPinReady=false;
bool recoveryButtonDown = false;
bool recoveryButtonFired = false;
uint32_t recoveryButtonStarted = 0;
char sessionToken[SessionManager::kTokenChars + 1] = {};
char qrPayload[128] = {};

bool runSessionSelfTest() {
  SessionManager test;
  char first[SessionManager::kTokenChars + 1] = {};
  char second[SessionManager::kTokenChars + 1] = {};
  const uint32_t start = millis();
  if (!test.createSession(SessionType::Access, 100, start, first) ||
      !test.validateSession(SessionType::Access, first, start) ||
      !test.createSession(SessionType::Access, 100, start, second) ||
      strcmp(first, second) == 0 ||
      test.validateSession(SessionType::Access, first, start) ||
      !test.validateSession(SessionType::Access, second, start) ||
      test.validateSession(SessionType::Management, second, start) ||
      !test.consumeSession(SessionType::Access, second, start) ||
      test.consumeSession(SessionType::Access, second, start) ||
      !test.createSession(SessionType::Access, 100, start, first) ||
      test.validateSession(SessionType::Access, first, start + 100) ||
      !test.createSession(SessionType::Access, 100, start, first)) return false;
  test.invalidateAllOfType(SessionType::Access);
  if (test.validateSession(SessionType::Access, first, start) ||
      !test.createSession(SessionType::Access, 30000, start, first)) return false;
  if (!test.createSession(SessionType::Enrollment, 120000, start, second) ||
      !test.validateSession(SessionType::Enrollment, second, start) ||
      test.validateSession(SessionType::Enrollment, second, start + 120000) ||
      !test.createSession(SessionType::Enrollment, 120000, start, second) ||
      !test.consumeSession(SessionType::Enrollment, second, start) ||
      test.consumeSession(SessionType::Enrollment, second, start)) return false;
  if (!test.createSession(SessionType::Reset, 90000, start, second) ||
      !test.validateSession(SessionType::Reset, second, start) ||
      test.validateSession(SessionType::Access, second, start) ||
      !test.consumeSession(SessionType::Reset, second, start) ||
      test.consumeSession(SessionType::Reset, second, start)) return false;
  AppStateMachine transitionTest;
  AppStateMachine unconfiguredTest;
  unconfiguredTest.begin(start, false);
  unconfiguredTest.onSingleTap(start);
  if (unconfiguredTest.state() != AppState::SetupRequest) return false;
  unconfiguredTest.onTripleTap(start + 1000);
  if (unconfiguredTest.state() != AppState::SetupRequest) return false;
  transitionTest.begin(start, true);
  transitionTest.onSingleTap(start);
  if (transitionTest.state() != AppState::AccessRequest) return false;
  transitionTest.onHold(start + 1000);
  if (transitionTest.state() != AppState::ManagementRequest) return false;
  transitionTest.onSingleTap(start + 1100);
  if (transitionTest.state() != AppState::ManagementRequest) return false;
  transitionTest.onTapReleased(start + 1150);
  if (transitionTest.state() != AppState::AccessRequest) return false;
  transitionTest.onHold(start + 1200);
  if (transitionTest.state() != AppState::ManagementRequest) return false;
  transitionTest.onSingleTap(start + 1300);
  if (transitionTest.state() != AppState::ManagementRequest) return false;
  transitionTest.onHold(start + 2000);
  if (transitionTest.state() != AppState::ResetRequest) return false;
  transitionTest.resetChoice(false, start + 2100);
  if (transitionTest.state() != AppState::AccessRequest) return false;
  transitionTest.onHold(start + 2200);
  transitionTest.onHold(start + 2300);
  transitionTest.resetChoice(true, start + 2400);
  if (transitionTest.state() != AppState::ResetConfirm) return false;
  test.invalidateAllOfType(SessionType::Access);
  if (test.validateSession(SessionType::Access, first, start + 1000) ||
      !test.createSession(SessionType::Management, 90000, start + 1000, second) ||
      !test.validateSession(SessionType::Management, second, start + 1000)) return false;
  transitionTest.update(start + 93000);
  return transitionTest.state() == AppState::IdleScreenOff;
}

void renderCurrentState(uint32_t) {
  const AppState state = app.state();
  if (state == AppState::ResetRequest || state == AppState::ResetConfirm ||
      state == AppState::Resetting) {
    sessions.invalidateAllOfType(SessionType::Access);
    sessions.invalidateAllOfType(SessionType::Management);
    sessions.invalidateAllOfType(SessionType::Reset);
    displayManager.render(state);
    return;
  }
  if (state == AppState::IdleScreenOff) {
    sessions.invalidateAllOfType(SessionType::Setup);
    sessions.invalidateAllOfType(SessionType::Access);
    sessions.invalidateAllOfType(SessionType::Management);
    sessions.invalidateAllOfType(SessionType::Reset);
    displayManager.render(state);
    return;
  }
  SessionType type = SessionType::Setup;
  const char* path = "setup";
  const char* label = "SETUP";
  uint32_t ttl = 300000;
  uint16_t screenColor = state == AppState::SetupRequest ? TFT_BLUE : TFT_WHITE;
  if (state == AppState::AccessRequest) {
    network.requestLocalFallback(millis());
    sessions.invalidateAllOfType(SessionType::Management);
    sessions.invalidateAllOfType(SessionType::Reset);
    type = SessionType::Access;
    path = "access";
    label = "ACCESS";
    ttl = 30000;
  } else if (state == AppState::ManagementRequest) {
    network.requestLocalFallback(millis());
    sessions.invalidateAllOfType(SessionType::Access);
    type = SessionType::Management;
    path = "manage";
    label = "MANAGEMENT";
    ttl = 90000;
    screenColor = TFT_YELLOW;
    sessions.invalidateAllOfType(SessionType::Reset);
  } else if (state == AppState::ResetRequest) {
    sessions.invalidateAllOfType(SessionType::Access);
    sessions.invalidateAllOfType(SessionType::Management);
    type = SessionType::Reset;
    path = "reset";
    label = "RESET";
    ttl = 90000;
    screenColor = TFT_RED;
  }
  if (!sessions.createSession(type, ttl, millis(), sessionToken)) {
    displayManager.render(AppState::ErrorStatus);
    return;
  }
  int length = 0;
  if (state == AppState::AccessRequest || state == AppState::ResetRequest) {
    length = snprintf(qrPayload, sizeof(qrPayload),
                      state == AppState::AccessRequest ? "http://%s/a/%s" :
                          "http://%s/reset?session=%s",
                      CanonicalOrigin::host(), sessionToken);
  } else {
    length = snprintf(qrPayload, sizeof(qrPayload),
                      "http://%s/%s?session=%s", CanonicalOrigin::host(), path, sessionToken);
  }
  memset(sessionToken, 0, sizeof(sessionToken));
  const bool rendered = length >= 0 &&
                        static_cast<size_t>(length) < sizeof(qrPayload) &&
                        qr.render(tft, qrPayload, label, screenColor);
  memset(qrPayload, 0, sizeof(qrPayload));
  if (!rendered) {
    displayManager.render(AppState::ErrorStatus);
    return;
  }
  displayManager.backlightOn();
  Serial.printf("SCREEN: %s QR\n", label);

}

void systemDiagnostic() {
  Preferences calibration;
  const bool calibrationLoaded = calibration.begin("lock-touch-v2", true) &&
      calibration.getBytesLength("cal") == 10;
  calibration.end();
  Serial.printf("SYSTEM INSPECT: configured=%u owner=%u unlock_ms=%u sd=%u calibration=%u pin_store=%u locked=%u gpio22=%u sta=%u admin_active=%u\n",
      configStore.healthy() && configStore.config().configured,
      configStore.healthy() && configStore.config().ownerExists,
      (unsigned)configStore.config().unlockDurationMs, storageHealth.available(),
      calibrationLoaded, adminPinReady, lockController.isLocked(),
      digitalRead(LockController::kPin) == LockController::kLockedLevel,
      network.staState() == NetworkManager::StaState::Connected, physicalAdmin.active());
  Serial.printf("NETWORK INSPECT: sta_ip=%s rssi=%ld ap_enabled=%u ap_clients=%u mdns=%u host=%s http=%u\n",
      network.staIp().toString().c_str(),(long)network.staRssi(),network.apEnabled(),
      WiFi.softAPgetStationNum(),CanonicalOrigin::ready(),CanonicalOrigin::host(),httpReady);
  Serial.printf("SYSTEM SESSIONS: access=%u management_qr=%u management_auth=%u enrollment=%u\n",
      sessions.active(SessionType::Access,millis()), sessions.active(SessionType::Management,millis()),
      sessions.active(SessionType::ManagementAuth,millis()), sessions.active(SessionType::Enrollment,millis()));
  Serial.println("SYSTEM INSPECT END");
  Serial.printf("INSTALLATION: healthy=%u id=%s unit=%s\n",
      smartlock::storage::InstallationMetadata::healthy(),
      smartlock::storage::InstallationMetadata::id(),
      smartlock::storage::InstallationMetadata::unitId());
  Serial.printf("HEAP: free=%u min=%u largest=%u\n",ESP.getFreeHeap(),ESP.getMinFreeHeap(),
      heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  LineNotifications::Status line = {};
  LineNotifications::status(line);
  Serial.printf("LINE INSPECT: configured=%u enabled=%u config_version=%u test_state=%u queue=%u sent=%u failed=%u state=%u\n",
      line.configured,line.enabled,(unsigned)line.configVersion,(unsigned)line.testState,
      (unsigned)line.queued,(unsigned)line.sentThisRun,(unsigned)line.failedThisRun,(unsigned)line.state);
  LineNotifications::Telemetry lt={};LineNotifications::telemetry(lt);
  Serial.printf("LINE PIPELINE: unlock=%u pin=%u emergency=%u enqueued=%u push_started=%u tls_connected=%u push_written=%u responses=%u heap_blocked=%u last_push_http=%u admission_free=%u admission_largest=%u admission_min=%u tls_min=%u tls_after_largest=%u\n",
    lt.unlockGenerated,lt.pinGenerated,lt.emergencyGenerated,lt.enqueued,lt.pushStarted,lt.tlsConnected,lt.pushWritten,lt.responses,lt.heapBlocked,lt.lastPushHttp,lt.admissionFree,lt.admissionLargest,lt.admissionMinimum,lt.tlsMinimum,lt.tlsAfterLargest);
  // Migration evidence only: read metadata/digest of inert history, never append/export/delete.
  File oldHistory = SD.open("/smartlock/outbox");
  uint32_t files=0, bytes=0, digest=2166136261UL;
  if(oldHistory && oldHistory.isDirectory()) {
    File item;
    while((item=oldHistory.openNextFile()) && files<64) {
      if(!item.isDirectory()) {
        ++files; bytes+=item.size();
        uint8_t chunk[128]; size_t remaining=item.size();
        while(remaining && remaining<=8192) {
          const int n=item.read(chunk,remaining<sizeof(chunk)?remaining:sizeof(chunk));
          if(n<=0)break;
          for(int i=0;i<n;++i)digest=(digest^chunk[i])*16777619UL;
          remaining-=n;
        }
      }
      item.close();
    }
  }
  oldHistory.close();
  Serial.printf("INERT HISTORY: files=%u bytes=%u digest=%08x writes=DISABLED\n",
      (unsigned)files,(unsigned)bytes,(unsigned)digest);
}
}

void setup() {
  lockController.begin();
  Serial.begin(115200);
  Serial.println("BOOT");
  Serial.println("SmartLock core product (USB updates)");
  Serial.println("LockController: LOCKED");
  systemFault = digitalRead(LockController::kPin) != LockController::kLockedLevel;
  Serial.printf("GPIO22: %s\n", systemFault ? "FAULT" : "LOCKED");
  Serial.printf("LOCK TIMER: %s\n", lockController.readyForUnlock() ? "OK" : "FAIL");

  const ConfigStore::Result configResult = configStore.begin();
  if (configResult == ConfigStore::Result::Ok ||
      configResult == ConfigStore::Result::CreatedDefaults) {
    Serial.printf("Config schema: %u\n", configStore.config().schemaVersion);
    Serial.printf("Configured: %s\n", configStore.config().configured ? "YES" : "NO");
    Serial.printf("Owner exists: %s\n", configStore.config().ownerExists ? "YES" : "NO");
  } else {
    Serial.println("Config: FAIL (locked)");
  }

  displayManager.begin(tft);
  pinMode(kRecalibratePin, INPUT_PULLUP);
  // A short BOOT press retains touch recalibration. Holding five seconds
  // requests network recovery only; it never erases data or unlocks.
  const bool bootPressed = digitalRead(kRecalibratePin) == LOW;
  const uint32_t bootHoldStarted = millis();
  while (bootPressed && digitalRead(kRecalibratePin) == LOW &&
         static_cast<uint32_t>(millis() - bootHoldStarted) < 5000) delay(10);
  const bool physicalRecovery = bootPressed &&
      static_cast<uint32_t>(millis() - bootHoldStarted) >= 5000;
  const bool forceCalibration = bootPressed && !physicalRecovery;
  tft.init();
  tft.setRotation(0);
  Serial.println("TFT: OK");

  uint16_t calibration[5] = {};
  preferences.begin("lock-touch-v2", false);
  const bool stored = preferences.isKey("cal") &&
                      preferences.getBytesLength("cal") == sizeof(calibration);
  if (stored && !forceCalibration) {
    preferences.getBytes("cal", calibration, sizeof(calibration));
    tft.setTouch(calibration);
    Serial.println("Touch calibration loaded");
  } else {
    displayManager.backlightOn();
    Serial.println("Touch calibration required");
    tft.calibrateTouch(calibration, TFT_WHITE, TFT_BLACK, 15);
    preferences.putBytes("cal", calibration, sizeof(calibration));
    Serial.println("Touch calibration saved");
  }
  preferences.end();
  touchManager.begin(tft);
  Serial.println("Touch: OK");
  const bool mayRepairSetup = configStore.healthy() &&
                              !configStore.config().configured &&
                              !configStore.config().ownerExists;
  if (storageHealth.begin(mayRepairSetup)) {
    runPhase2SelfTest();
  } else systemFault=true;
  Serial.printf("SESSION TEST: %s\n", runSessionSelfTest() ? "PASS" : "FAIL");
  const bool configClaimsConfigured = configStore.healthy() &&
                                      configStore.config().configured &&
                                      configStore.config().ownerExists;
  const bool migrationValid = !configClaimsConfigured || (storageHealth.available() &&
      smartlock::storage::IdentityStore::migrateLegacy());
  const bool configured = configClaimsConfigured && migrationValid && firstOwner.configuredDataValid();
  if (configClaimsConfigured) Serial.printf("IDENTITY MODEL: %s OWNER_VERIFIER_PRESERVED=%u\n",
      migrationValid ? "PASS" : "FAIL", smartlock::storage::IdentityStore::backupVerifierUnchanged());
  size_t usersCount = 0, devicesCount = 0, activeCount = 0, revokedCount = 0;
  const bool databaseValid = !configured ||
      enrollment.databaseIntegrity(usersCount, devicesCount, activeCount, revokedCount);
  if (configured) Serial.printf("IDENTITY DB: %s identities=%u owners=%u active=%u revoked=%u\n",
                                databaseValid ? "PASS" : "FAIL", (unsigned)usersCount,
                                (unsigned)devicesCount, (unsigned)activeCount,
                                (unsigned)revokedCount);
  const bool setupAllowed = configStore.healthy() &&
                            !configStore.config().configured &&
                            !configStore.config().ownerExists;
  systemFault = systemFault || (!setupAllowed && !(configured && databaseValid));
  char apPassword[33] = {};
  bool apStarted = false;
  if (setupAllowed) apStarted = network.beginSetupAp();
  if (configured && databaseValid &&
      networkSecrets.load(apPassword) == NetworkSecrets::Result::Valid)
    apStarted = network.beginConfiguredAp(apPassword);
  memset(apPassword, 0, sizeof(apPassword));
  if (apStarted) {
    if (physicalRecovery && configured && !network.enterRecovery())
      Serial.println("NETWORK: RECOVERY REQUEST FAILED (normal AP retained)");
    Serial.printf("AP: OK SSID %s IP %s\n", network.ssid(),
                  network.ip().toString().c_str());
    Serial.printf("DNS: %s\n", portal.begin(network.ip()) ? "OK" : "FAIL");
    Serial.printf("CANONICAL: http://%s MDNS: %s\n", CanonicalOrigin::host(),
                  (canonicalReady=CanonicalOrigin::begin()) ? "OK" : "FAIL (numeric recovery available)");
    httpReady=web.begin(configured, &firstOwner, network.ssid(), &enrollment, &network, &accessController, &factoryReset, &lockController, &configStore);
    Serial.printf("HTTP: %s\n",httpReady ? "OK" : "FAIL");
    if (configured && databaseValid) {
      Serial.printf("STA saved config: %s\n", network.startSavedSta(millis()) ? "CONNECTING" : "NONE");
    }
  } else if (systemFault) {
    Serial.println("SYSTEM: FAIL (configuration/database)");
  } else {
    Serial.println("AP: FAIL");
  }
  // Only an affirmative empty-installation state may create the temporary PIN.
  // A failed SD/AP/migration check on a configured installation is not new setup.
  auto absentNvsKey = [](const char* name, const char* key) {
    nvs_handle_t handle = 0;
    const esp_err_t opened = nvs_open(name, NVS_READONLY, &handle);
    if (opened == ESP_ERR_NVS_NOT_FOUND) return true;
    if (opened != ESP_OK) return false;
    size_t length = 0;
    const esp_err_t found = nvs_get_blob(handle, key, nullptr, &length);
    nvs_close(handle);
    return found == ESP_ERR_NVS_NOT_FOUND;
  };
  const bool noPriorNvs = absentNvsKey("sl-install", "id") &&
      absentNvsKey("sl-pin", "initial") && absentNvsKey("sl-pin", "fail") &&
      absentNvsKey("sl-net", "ap-pass") &&
      absentNvsKey("sl-sta", "cfg-a") && absentNvsKey("sl-sta", "cfg-b");
  // sl-line is intentionally not part of this check: Factory Reset preserves the
  // prototype LINE configuration (like touch calibration), so it is device-level
  // state rather than evidence of a prior Owner installation.
  size_t initialAuthCount = 0;
  const bool freshInstallation = configResult == ConfigStore::Result::CreatedDefaults &&
      setupAllowed && storageHealth.available() && noPriorNvs &&
      !smartlock::storage::IdentityStore::modePresent() &&
      AuthStore::inspect(initialAuthCount) == AuthStore::ReadResult::Missing &&
      !SD.exists("/smartlock/db/users.rec") && !SD.exists("/smartlock/db/users.rec.bak") &&
      !SD.exists("/smartlock/db/users.rec.tmp") &&
      !SD.exists("/smartlock/db/devices.rec") && !SD.exists("/smartlock/db/devices.rec.bak") &&
      !SD.exists("/smartlock/db/devices.rec.tmp") &&
      !SD.exists("/smartlock/db/auth.rec.bak") && !SD.exists("/smartlock/db/auth.rec.tmp");
  // Local emergency authorization needs local security readiness, not a working
  // AP, mDNS responder or HTTP server. Runtime storage is checked again below.
  safeBootInitialized=!systemFault&&configured&&databaseValid&&lockController.readyForUnlock()&&
      lockController.isLocked()&&digitalRead(LockController::kPin)==LockController::kLockedLevel;
  const bool pinReady=AdminPin::begin(configured, freshInstallation);
  adminPinReady=pinReady;
  if(!pinReady){systemFault=true;safeBootInitialized=false;}
  Serial.printf("ADMIN PIN STORE: %s\n",pinReady?"OK":"FAIL (LOCKED)");
  const bool installationReady = smartlock::storage::InstallationMetadata::begin(configured);
  Serial.printf("INSTALLATION STORE: %s\n",installationReady?"OK":"UNAVAILABLE");
  Diagnostics::setSystemDiagnostic(systemDiagnostic);
  AdminPin::setFailureCallback([](uint8_t count, bool lockout) {
    LineNotifications::pinFailed(count, lockController.isLocked(), lockout);
  });
  LineNotifications::begin(network.staState() == NetworkManager::StaState::Connected);
  Serial.println("LINE: READY (Owner Management + ADMIN PIN)");
  Serial.printf("HEAP: free=%u min=%u\n", (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMinFreeHeap());
  app.begin(millis(), configured);
  if (systemFault) displayManager.render(AppState::ErrorStatus);
  else renderCurrentState(millis());
  app.acknowledgeChange();
  Serial.println("READY");
}

void loop() {
  const uint32_t now = millis();
  lockController.update(now);
  // BOOT can also be held after normal startup (holding GPIO0 across reset
  // selects the ESP32 ROM loader). This nonblocking path is usable at runtime.
  if (digitalRead(kRecalibratePin) == LOW) {
    if (!recoveryButtonDown) {
      recoveryButtonDown = true;
      recoveryButtonStarted = now;
    }
    if (!recoveryButtonFired &&
        static_cast<uint32_t>(now - recoveryButtonStarted) >= 5000) {
      recoveryButtonFired = true;
      lockController.lock();
      sessions.invalidateAllOfType(SessionType::Access);
      network.enterRecovery();
    }
  } else {
    recoveryButtonDown = recoveryButtonFired = false;
  }
  network.update(now);
  CanonicalOrigin::update(millis(),network.staIp(),network.apEnabled()?network.ip():IPAddress(0,0,0,0));
  const bool staConnected = network.staState() == NetworkManager::StaState::Connected;
  TimeManager::update(staConnected, now);
  LineNotifications::update(staConnected);
  if (firstOwner.restartDue(now)) ESP.restart();
  if (factoryReset.restartDue(now)) {
    lockController.lock();
    WiFi.disconnect(true, true);
    ESP.restart();
  }
  portal.update();
  web.handleClient();
  // Retire temporary authority even while the physical Admin screen is open.
  sessions.expireSessions(millis());
  Diagnostics::poll();
  if (factoryReset.restartPending()) { delay(5); return; }
  const TouchEvent event = touchManager.update(millis());
  if (systemFault) {
    delay(5);
    return;
  }
  if(!physicalAdmin.active()&&event==TouchEvent::Hold&&app.state()==AppState::ManagementRequest){
      lockController.lock();
      sessions.invalidateAllOfType(SessionType::Access);
      sessions.invalidateAllOfType(SessionType::Management);
  }
  TouchEvent routedEvent=event;
  const auto action=adminTouchRouter.dispatch(physicalAdmin,app,
      routedEvent,
      touchManager.x(),touchManager.y(),millis());
  if(physicalAdmin.active()&&physicalAdmin.dirty())renderAdmin(tft,physicalAdmin);
  if(action==PhysicalAdmin::Action::Unlock){
    // Only the PIN-authenticated explicit confirmation emits this local action.
    size_t authCount=0;
    bool granted=safeBootInitialized&&storageHealth.available()&&
      lockController.isLocked()&&digitalRead(LockController::kPin)==LockController::kLockedLevel&&
      smartlock::storage::IdentityStore::healthy()&&AuthStore::inspect(authCount)==AuthStore::ReadResult::Valid&&
      lockController.unlock(configStore.config().unlockDurationMs)==LockController::UnlockResult::Unlocked;
    if(granted) LineNotifications::emergencyUnlock();
  }else if(action==PhysicalAdmin::Action::Reset){
    lockController.lock();
    if(factoryReset.requestPhysical(true)!=FactoryResetController::Result::ResetScheduled)systemFault=true;
  }
  if(physicalAdmin.active()){delay(5);return;}
  app.update(millis());
  if (app.changed()) {
    renderCurrentState(millis());
    app.acknowledgeChange();
  }
  delay(5);
}
