#pragma once

#include <WebServer.h>
class FirstOwnerSetup;
class EnrollmentManager;
class NetworkManager;
class AccessController;
class FactoryResetController;
class LockController;
class ConfigStore;

class WebServerManager {
 public:
  WebServerManager();
  bool begin(bool configured, FirstOwnerSetup* setup, const char* apSsid,
             EnrollmentManager* enrollment = nullptr,
             NetworkManager* network = nullptr,
             AccessController* access = nullptr,
             FactoryResetController* reset = nullptr,
             LockController* lock = nullptr, ConfigStore* config = nullptr);
  void handleClient();

 private:
  void sendStatus();
  void redirectToHome();
  void completeSetup();
  void reconcileRegistration();
  bool copyArg(const char* key, char* output, size_t capacity);
  bool managementAuthorized();
  bool homeLanRequest();
  bool requireEnrollmentLan();
  bool validPost(size_t fields, size_t maxBodyBytes = 512);
  void managementLogin();
  void managementState();
  void managementSummary();
  bool lineOwnerAuthorized();
  void lineStatus();
  void lineLabel();
  void lineTest();
  void lineDisconnect();
  void lineMaintenanceArm();
  void lineMaintenanceCommit();
  void lineMaintenanceCancel();
  void clearLineMaintenance();
  void managementAddUser();
  void managementEnroll();
  void managementRevoke();
  void managementUserStatus();
  void enrollmentCheck();
  void enrollmentCredentialStatus();
  void enrollmentComplete();
  void networkStatus();
  void networkScan();
  void networkConnect();
  void handoffCreate();
  void handoffComplete();
  void bootstrapCreate();
  void bootstrapComplete();
  void ownerVerify();
  void accessRequest();
  void factoryResetConfirm();
  bool ownerPinAuthorized();
  void adminPinChange();
  void unlockDuration();
  // Same bounds as ConfigStore validation and FirstOwnerSetup.
  static constexpr uint32_t kMinUnlockSeconds = 1, kMaxUnlockSeconds = 60;
  bool sensitiveRateLimit();

  WebServer server_;
  bool configured_ = false;
  bool started_ = false;
  FirstOwnerSetup* setup_ = nullptr;
  EnrollmentManager* enrollment_ = nullptr;
  NetworkManager* network_ = nullptr;
  AccessController* access_ = nullptr;
  FactoryResetController* reset_ = nullptr;
  LockController* lock_ = nullptr;
  ConfigStore* config_ = nullptr;
  uint32_t rateStarted_ = 0;
  uint8_t rateCount_ = 0;
  uint32_t pinRateStarted_ = 0;
  uint8_t pinRateCount_ = 0;
  char lineMaintenanceNonce_[33] = {};
  char lineMaintenanceOwnerToken_[65] = {};
  uint32_t lineMaintenanceStarted_ = 0;
  uint32_t lineMaintenancePinRevision_ = 0;
  char apSsid_[33] = {};
};
