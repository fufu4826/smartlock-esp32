#include "WebServerManager.h"

#include <Arduino.h>
#include "../security/RequestPolicy.h"

#include "../web/WebAssets.h"
#include "../app/FirstOwnerSetup.h"
#include "../app/RegistrationRecovery.h"
#include "../app/EnrollmentManager.h"
#include "NetworkManager.h"
#include "CanonicalOrigin.h"
#include "../app/AccessController.h"
#include "../app/FactoryResetController.h"
#include "../hardware/LockController.h"
#include <WiFi.h>
#include <memory>
#include <new>
#include <esp_system.h>
#include <QRCode.h>
#include "../storage/AuthStore.h"
#include "../security/AdminPin.h"
#include "../storage/ConfigStore.h"
#include "../notifications/LineNotifications.h"

namespace {
void appendJson(String& output, const char* value) {
  output += '"';
  if (value) for (const unsigned char* p = (const unsigned char*)value; *p; ++p) {
    if (*p == '"' || *p == '\\') { output += '\\'; output += (char)*p; }
    else if (*p >= 0x20) output += (char)*p;
  }
  output += '"';
}
bool enrollmentSvg(const char* url, String& svg) {
  uint8_t buffer[512] = {}; QRCode qr;
  if (qrcode_getBufferSize(6) > sizeof(buffer) ||
      qrcode_initText(&qr, buffer, 6, ECC_LOW, url) < 0) return false;
  if (!svg.reserve(16000)) return false;
  svg = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 49 49' shape-rendering='crispEdges'><path fill='white' d='M0 0h49v49H0z'/><path fill='black' d='";
  for (int y = 0; y < qr.size; ++y) for (int x = 0; x < qr.size;) {
    if (!qrcode_getModule(&qr, x, y)) { ++x; continue; }
    const int start = x++;
    while (x < qr.size && qrcode_getModule(&qr, x, y)) ++x;
    char run[40]; snprintf(run, sizeof(run), "M%d %dh%dv1h-%dz", start+4, y+4, x-start, x-start);
    svg += run;
  }
  svg += "'/></svg>"; return true;
}
}

WebServerManager::WebServerManager() : server_(80) {}

bool WebServerManager::begin(bool configured, FirstOwnerSetup* setup, const char* apSsid,
                             EnrollmentManager* enrollment, NetworkManager* network,
                             AccessController* access,
                             FactoryResetController* reset, LockController* lock, ConfigStore* config) {
  if (started_) return true;
  configured_ = configured;
  setup_ = setup;
  enrollment_ = enrollment;
  network_ = network;
  access_ = access;
  reset_ = reset;
  lock_ = lock;
  config_ = config;
  if (apSsid) snprintf(apSsid_, sizeof(apSsid_), "%s", apSsid);
  const char* capturedHeaders[] = {"Content-Type"};
  server_.collectHeaders(capturedHeaders, 1);

  server_.on("/", HTTP_GET, [this]() {
    server_.send_P(200, "text/html; charset=utf-8", WebAssets::kIndex);
  });
  // The QR query (including its session token) is deliberately ignored.
  server_.on("/setup", HTTP_GET, [this]() {
    server_.send_P(200, "text/html; charset=utf-8", WebAssets::kSetup);
  });
  server_.on("/api/status", HTTP_GET, [this]() { sendStatus(); });
  server_.on("/api/system/admin-pin", HTTP_POST, [this]() { adminPinChange(); });
  server_.on("/api/manage/unlock-duration", HTTP_POST, [this]() { unlockDuration(); });
  server_.on("/api/setup/complete", HTTP_POST, [this]() { completeSetup(); });
  server_.on("/api/registration/reconcile", HTTP_POST, [this]() { reconcileRegistration(); });
  server_.on("/manage", HTTP_GET, [this]() {
    server_.sendHeader("Cache-Control", "no-store");
    server_.sendHeader("Referrer-Policy", "no-referrer");
    server_.send_P(configured_ ? 200 : 404, "text/html; charset=utf-8",
                   configured_ ? WebAssets::kManage : WebAssets::kIndex);
  });
  server_.on("/enroll", HTTP_GET, [this]() {
    server_.sendHeader("Cache-Control", "no-store");
    server_.sendHeader("Referrer-Policy", "no-referrer");
    server_.send_P(configured_ ? 200 : 404, "text/html; charset=utf-8",
                   configured_ ? WebAssets::kEnroll : WebAssets::kIndex);
  });
  server_.on("/api/manage/login", HTTP_POST, [this]() { managementLogin(); });
  server_.on("/api/manage/state", HTTP_POST, [this]() { managementState(); });
  server_.on("/api/manage/summary", HTTP_POST, [this]() { managementSummary(); });
  server_.on("/api/manage/users", HTTP_POST, [this]() { managementAddUser(); });
  server_.on("/api/manage/enroll", HTTP_POST, [this]() { managementEnroll(); });
  server_.on("/api/manage/revoke", HTTP_POST, [this]() { managementRevoke(); });
  server_.on("/api/manage/user-status", HTTP_POST, [this]() { managementUserStatus(); });
  server_.on("/api/enroll/credential-status", HTTP_POST, [this]() { enrollmentCredentialStatus(); });
  server_.on("/api/enroll/check", HTTP_GET, [this]() { enrollmentCheck(); });
  server_.on("/api/enroll/complete", HTTP_POST, [this]() { enrollmentComplete(); });
  server_.on("/api/network/status", HTTP_POST, [this]() { networkStatus(); });
  server_.on("/api/network/scan", HTTP_POST, [this]() { networkScan(); });
  server_.on("/api/network/connect", HTTP_POST, [this]() { networkConnect(); });
  server_.on("/api/line/status", HTTP_POST, [this]() { lineStatus(); });
  server_.on("/api/line/label", HTTP_POST, [this]() { lineLabel(); });
  server_.on("/api/line/test", HTTP_POST, [this]() { lineTest(); });
  server_.on("/api/line/disconnect", HTTP_POST, [this]() { lineDisconnect(); });
  server_.on("/api/line/maintenance/arm", HTTP_POST, [this]() { lineMaintenanceArm(); });
  server_.on("/api/line/maintenance/commit", HTTP_POST, [this]() { lineMaintenanceCommit(); });
  server_.on("/api/line/maintenance/cancel", HTTP_POST, [this]() { lineMaintenanceCancel(); });
  server_.on("/api/network/bootstrap/create", HTTP_POST, [this]() { bootstrapCreate(); });
  server_.on("/api/network/bootstrap/complete", HTTP_POST, [this]() { bootstrapComplete(); });
  server_.on("/verify-owner", HTTP_GET, [this]() { server_.send(410, "text/plain", "Retired"); });
  server_.on("/api/owner/verify", HTTP_POST, [this]() { ownerVerify(); });
  server_.on("/api/access/request", HTTP_POST, [this]() { accessRequest(); });
  server_.on("/owner-bootstrap", HTTP_GET, [this]() { server_.send(410, "text/plain", "Retired"); });
  server_.on("/health", HTTP_GET, [this]() {
    homeLanRequest();
    server_.send(200, "text/plain; charset=utf-8", "ok\n");
  });

  server_.on("/generate_204", HTTP_GET, [this]() { redirectToHome(); });
  server_.on("/gen_204", HTTP_GET, [this]() { redirectToHome(); });
  server_.on("/hotspot-detect.html", HTTP_GET, [this]() { redirectToHome(); });
  server_.on("/connecttest.txt", HTTP_GET, [this]() { redirectToHome(); });
  server_.on("/ncsi.txt", HTTP_GET, [this]() { redirectToHome(); });
  server_.onNotFound([this]() {
    if(server_.uri().startsWith("/api/system/")){server_.send(404,"text/plain; charset=utf-8","ไม่พบหน้าที่ต้องการ\n");return;}
    const String uri = server_.uri();
    if (configured_ && server_.method() == HTTP_GET && uri.startsWith("/a/") &&
        uri.length() == 67) {
      server_.sendHeader("Cache-Control", "no-store");
      server_.sendHeader("Referrer-Policy", "no-referrer");
      server_.send_P(200, "text/html; charset=utf-8", WebAssets::kAccess);
      return;
    }
    if (server_.method() == HTTP_GET) {
      server_.sendHeader("Location", "/setup", true);
      server_.send(302, "text/plain; charset=utf-8", "ไปยังหน้าตั้งค่า\n");
    } else {
      server_.send(404, "text/plain; charset=utf-8", "ไม่พบหน้าที่ต้องการ\n");
    }
  });

  server_.begin();
  started_ = true;
  return true;
}

void WebServerManager::handleClient() {
  if (lineMaintenanceNonce_[0] &&
      (uint32_t(millis() - lineMaintenanceStarted_) >= 120000 ||
       lineMaintenancePinRevision_ != AdminPin::revision() ||
       !lock_ || !lock_->isLocked() || lock_->remainingUnlockMs(millis())))
    clearLineMaintenance();
  if (started_) { server_.handleClient(); server_.eraseRequestArguments(); }
}


void WebServerManager::sendStatus() {
  homeLanRequest();
  char body[160];
  snprintf(body, sizeof(body),
           "{\"configured\":%s,\"state\":\"%s\",\"canonicalHost\":\"%s\"}\n",
           configured_ ? "true" : "false",
           configured_ ? "ready" : "setup", CanonicalOrigin::host());
  server_.send(200, "application/json; charset=utf-8", body);
}

void WebServerManager::redirectToHome() {
  server_.sendHeader("Location", "/", true);
  server_.send(302, "text/plain; charset=utf-8", "เครือข่ายสำหรับตั้งค่า SmartLock\n");
}

bool WebServerManager::copyArg(const char* key, char* output, size_t capacity) {
  if (!server_.hasArg(key) || !output || capacity == 0) return false;
  const String value = server_.arg(key);
  if (value.length() == 0 || value.length() >= capacity) return false;
  if (memchr(value.c_str(), '\0', value.length()) != nullptr) return false;
  memset(output, 0, capacity);
  memcpy(output, value.c_str(), value.length());
  if (!strcmp(key,"deviceId") && !RequestPolicy::deviceId(output)) return false;
  return true;
}

void WebServerManager::completeSetup() {
  if (!setup_ || configured_) {
    server_.send(409, "application/json", "{\"error\":\"setup_unavailable\"}");
    return;
  }
  if (server_.clientContentLength() <= 0 || server_.clientContentLength() > 1024 ||
      server_.args() != 7 ||
      !RequestPolicy::formContentType(server_.header("Content-Type").c_str())) {
    server_.send(400, "application/json", "{\"error\":\"invalid_request\"}");
    return;
  }
  FirstOwnerInput input = {};
  if (!copyArg("session", input.session, sizeof(input.session)) ||
      !copyArg("ownerName", input.ownerName, sizeof(input.ownerName)) ||
      !copyArg("credential", input.credential, sizeof(input.credential)) ||
      !copyArg("apPassword", input.apPassword, sizeof(input.apPassword)) ||
      !copyArg("adminPin", input.adminPin, sizeof(input.adminPin)) ||
      !copyArg("pinConfirm", input.pinConfirm, sizeof(input.pinConfirm)) ||
      !server_.hasArg("unlockSeconds")) {
    server_.send(400, "application/json", "{\"error\":\"invalid_fields\"}");
    return;
  }
  const String seconds = server_.arg("unlockSeconds");
  if (seconds.length() == 0 || seconds.length() > 2) {
    server_.send(400, "application/json", "{\"error\":\"invalid_duration\"}");
    return;
  }
  uint32_t duration = 0;
  for (size_t i = 0; i < seconds.length(); ++i) {
    if (seconds[i] < '0' || seconds[i] > '9') {
      server_.send(400, "application/json", "{\"error\":\"invalid_duration\"}");
      return;
    }
    duration = duration * 10 + (seconds[i] - '0');
  }
  input.unlockSeconds = duration;
  const auto result = setup_->complete(input, millis());
  memset(&input, 0, sizeof(input));
  if (result == FirstOwnerSetup::Result::Success) {
    configured_ = true;
    char response[96];
    snprintf(response, sizeof(response), "{\"ok\":true,\"apSsid\":\"%s\"}", apSsid_);
    server_.send(200, "application/json", response);
  } else if (result == FirstOwnerSetup::Result::InvalidInput) {
    server_.send(400, "application/json", "{\"error\":\"invalid_fields\"}");
  } else if (result == FirstOwnerSetup::Result::SessionRejected) {
    server_.send(403, "application/json", "{\"error\":\"setup_session_expired\"}");
  } else if (result == FirstOwnerSetup::Result::AlreadyConfigured ||
             result == FirstOwnerSetup::Result::DatabaseConflict) {
    server_.send(409, "application/json", "{\"error\":\"setup_conflict\"}");
  } else {
    server_.send(503, "application/json", "{\"error\":\"setup_storage_unavailable\"}");
  }
}

bool WebServerManager::validPost(size_t fields, size_t maxBodyBytes) {
  return configured_ && enrollment_ && server_.clientContentLength() > 0 &&
         server_.clientContentLength() <= maxBodyBytes && server_.args() == fields &&
         RequestPolicy::formContentType(server_.header("Content-Type").c_str());
}

void WebServerManager::reconcileRegistration() {
  if (!sensitiveRateLimit()) return;
  if (!validPost(2)) { server_.send(400,"application/json","{\"error\":\"invalid_request\"}"); return; }
  char kind[12]={}, credential[65]={}, id[9]={};
  smartlock::storage::UserRole role=smartlock::storage::UserRole::User;
  const bool fields=copyArg("kind",kind,sizeof(kind)) &&
      copyArg("credential",credential,sizeof(credential));
  const bool setup=fields && !strcmp(kind,"setup");
  const bool enrollment=fields && !strcmp(kind,"enrollment");
  // Existing credential proof recovers an existing identity only. An invitation
  // is neither accepted nor consumed, and no authorization data is written.
  const bool valid=(setup || enrollment) && config_ && config_->healthy() &&
      config_->config().configured && config_->config().ownerExists &&
      (setup || homeLanRequest()) && RegistrationRecovery::reconcile(
          setup ? RegistrationRecovery::Kind::Setup : RegistrationRecovery::Kind::Enrollment,
          credential,id,role);
  volatile char* erase=credential;
  for(size_t i=0;i<sizeof(credential);++i) erase[i]=0;
  server_.sendHeader("Cache-Control","no-store");
  server_.sendHeader("Referrer-Policy","no-referrer");
  if(!valid){server_.send(403,"application/json","{\"error\":\"unavailable\"}");return;}
  const char* roleText=role==smartlock::storage::UserRole::Owner ? "Owner" :
      role==smartlock::storage::UserRole::Admin ? "Admin" :
      role==smartlock::storage::UserRole::Guest ? "Guest" : "User";
  String body="{\"ok\":true,\"deviceId\":";appendJson(body,id);
  body+=",\"role\":";appendJson(body,roleText);
  if(setup){body+=",\"apSsid\":";appendJson(body,network_?network_->ssid():apSsid_);}
  body+='}';server_.send(200,"application/json",body);
}

bool WebServerManager::managementAuthorized() {
  char token[65] = {};
  const bool valid = configured_ && enrollment_ &&
      copyArg("token", token, sizeof(token)) &&
      (homeLanRequest() ? enrollment_->authorized(token, millis())
                        : enrollment_->authorizedOwner(token, millis()));
  memset(token, 0, sizeof(token));
  if (!valid) server_.send(403, "application/json", "{\"error\":\"forbidden\"}");
  return valid;
}

bool WebServerManager::homeLanRequest() {
  const bool lan = network_ && network_->staIp() != IPAddress(0,0,0,0) &&
      server_.client().localIP() == network_->staIp();
  if (lan) network_->noteLanRequest(millis());
  return lan;
}

bool WebServerManager::requireEnrollmentLan() {
  if (homeLanRequest()) return true;
  server_.send(403, "application/json", "{\"error\":\"home_lan_required\"}");
  return false;
}

void WebServerManager::managementLogin() {
  if (!sensitiveRateLimit()) return;
  if (!validPost(3)) { server_.send(400, "application/json", "{\"error\":\"invalid_request\"}"); return; }
  char session[65] = {}, deviceId[9] = {}, credential[65] = {}, token[65] = {};
  const bool valid = copyArg("session", session, sizeof(session)) &&
      copyArg("deviceId", deviceId, sizeof(deviceId)) &&
      copyArg("credential", credential, sizeof(credential)) &&
      (homeLanRequest() || enrollment_->isActiveOwnerDevice(deviceId)) &&
      enrollment_->login(session, deviceId, credential, millis(), token);
  memset(credential, 0, sizeof(credential)); memset(session, 0, sizeof(session));
  if (!valid) { server_.send(403, "application/json", "{\"error\":\"forbidden\"}"); return; }
  clearLineMaintenance();
  String body = "{\"ok\":true,\"token\":\"";
  body += token; body += "\"}";
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "application/json", body);
  memset(token, 0, sizeof(token));
}

void WebServerManager::managementState() {
  if (!validPost(1)) { server_.send(400, "application/json", "{\"error\":\"invalid_request\"}"); return; }
  if (!managementAuthorized()) return;
  String json;
  if (!enrollment_->snapshot(json)) { server_.send(503, "application/json", "{\"error\":\"storage\"}"); return; }
  json.remove(json.length()-1);
  json += ",\"actorRole\":"; appendJson(json, enrollment_->actorRole());
  json += ",\"lanEnrollment\":"; json += homeLanRequest() ? "true" : "false"; json += '}';
  server_.send(200, "application/json", json);
}

bool WebServerManager::lineOwnerAuthorized() {
  if (!managementAuthorized()) return false;
  char token[65] = {};
  const bool owner = copyArg("token", token, sizeof(token)) &&
      enrollment_->authorizedOwner(token, millis());
  memset(token, 0, sizeof(token));
  if (!owner) server_.send(403, "application/json", "{\"error\":\"owner_required\"}");
  return owner;
}

void WebServerManager::lineStatus() {
  if (!validPost(1)) { server_.send(400, "application/json", "{\"error\":\"invalid_request\"}"); return; }
  if (!lineOwnerAuthorized()) return;
  LineNotifications::Status status = {};
  LineNotifications::status(status);
  const char* state = "disabled";
  switch (status.state) {
    case LineNotifications::State::Disabled: state = "disabled"; break;
    case LineNotifications::State::Ready: state = "ready"; break;
    case LineNotifications::State::Offline: state = "offline"; break;
    case LineNotifications::State::QuotaFull: state = "quota_full"; break;
    case LineNotifications::State::AuthRequired: state = "auth_required"; break;
    case LineNotifications::State::ServiceError: state = "service_error"; break;
    case LineNotifications::State::TimeUnavailable: state = "time_unavailable"; break;
  }
  String body = "{\"ok\":true,\"configured\":"; body += status.configured ? "true" : "false";
  body += ",\"enabled\":"; body += status.enabled ? "true" : "false";
  body += ",\"staConnected\":"; body += status.staConnected ? "true" : "false";
  body += ",\"quotaKnown\":"; body += status.quotaKnown ? "true" : "false";
  body += ",\"queued\":"; body += String(status.queued);
  body += ",\"sentThisRun\":"; body += String(status.sentThisRun);
  body += ",\"failedThisRun\":"; body += String(status.failedThisRun);
  body += ",\"quotaLimit\":"; body += String(status.quotaLimit);
  body += ",\"quotaUsed\":"; body += String(status.quotaUsed);
  body += ",\"testState\":"; body += String(status.testState);
  body += ",\"configVersion\":"; body += String(status.configVersion);
  body += ",\"state\":"; appendJson(body, state);
  body += ",\"deviceLabel\":"; appendJson(body, status.deviceLabel);
  body += ",\"publicBasicId\":"; appendJson(body, status.publicBasicId);
  body += "}";
  server_.sendHeader("Cache-Control", "no-store");
  server_.sendHeader("Referrer-Policy", "no-referrer");
  server_.send(200, "application/json", body);
}

static void sendLineResult(WebServer& server, LineNotifications::Result result) {
  int code = 200;
  const char* body = "{\"ok\":true}";
  switch (result) {
    case LineNotifications::Result::Ok: break;
    case LineNotifications::Result::InvalidInput: code = 400; body = "{\"error\":\"invalid_fields\"}"; break;
    case LineNotifications::Result::NotConfigured: code = 409; body = "{\"error\":\"not_configured\"}"; break;
    case LineNotifications::Result::RateLimited: code = 429; body = "{\"error\":\"rate_limited\"}"; break;
    case LineNotifications::Result::StorageError: code = 503; body = "{\"error\":\"storage_unavailable\"}"; break;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Referrer-Policy", "no-referrer");
  server.send(code, "application/json", body);
}

void WebServerManager::lineLabel() {
  if (!validPost(2)) { server_.send(400, "application/json", "{\"error\":\"invalid_request\"}"); return; }
  if (!lineOwnerAuthorized()) return;
  char label[65] = {};
  const bool valid = copyArg("label", label, sizeof(label));
  const LineNotifications::Result result = valid
      ? LineNotifications::rename(label)
      : LineNotifications::Result::InvalidInput;
  memset(label, 0, sizeof(label));
  sendLineResult(server_, result);
}

void WebServerManager::lineTest() {
  if (!validPost(1)) { server_.send(400, "application/json", "{\"error\":\"invalid_request\"}"); return; }
  if (!lineOwnerAuthorized()) return;
  sendLineResult(server_, LineNotifications::requestTest());
}

void WebServerManager::lineDisconnect() {
  if (!validPost(1)) { server_.send(400, "application/json", "{\"error\":\"invalid_request\"}"); return; }
  if (!lineOwnerAuthorized()) return;
  clearLineMaintenance();
  sendLineResult(server_, LineNotifications::disconnect());
}

void WebServerManager::clearLineMaintenance() {
  memset(lineMaintenanceNonce_, 0, sizeof(lineMaintenanceNonce_));
  memset(lineMaintenanceOwnerToken_, 0, sizeof(lineMaintenanceOwnerToken_));
  lineMaintenanceStarted_ = 0;
  lineMaintenancePinRevision_ = 0;
}

void WebServerManager::lineMaintenanceArm() {
  if (!validPost(2, 256)) {
    server_.send(400, "application/json", "{\"error\":\"invalid_request\"}"); return;
  }
  // Owner authorization and locked-state checks happen before the PIN is read
  // or verified, so unauthenticated requests cannot affect the PIN counter.
  if (!ownerPinAuthorized()) return;
  clearLineMaintenance();

  char ownerToken[65] = {}, pin[5] = {};
  const bool fields = copyArg("token", ownerToken, sizeof(ownerToken)) &&
      copyArg("pin", pin, sizeof(pin));
  const AdminPin::Result result = fields && AdminPin::valid(pin)
      ? AdminPin::verify(pin, millis()) : AdminPin::Result::Invalid;
  memset(pin, 0, sizeof(pin));
  if (result != AdminPin::Result::Ok) {
    memset(ownerToken, 0, sizeof(ownerToken));
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(result == AdminPin::Result::Locked ? 429 : 403,
                 "application/json", "{\"error\":\"pin_rejected\"}");
    return;
  }

  uint8_t random[16] = {};
  for (size_t i = 0; i < sizeof(random); i += 4) {
    const uint32_t value = esp_random();
    memcpy(random + i, &value, sizeof(value));
  }
  for (size_t i = 0; i < sizeof(random); ++i)
    snprintf(lineMaintenanceNonce_ + i * 2, 3, "%02x", random[i]);
  memset(random, 0, sizeof(random));
  memcpy(lineMaintenanceOwnerToken_, ownerToken, sizeof(ownerToken));
  memset(ownerToken, 0, sizeof(ownerToken));
  lineMaintenanceStarted_ = millis();
  lineMaintenancePinRevision_ = AdminPin::revision();

  String body = "{\"ok\":true,\"nonce\":\"";
  body += lineMaintenanceNonce_;
  body += "\",\"expiresIn\":120}";
  server_.sendHeader("Cache-Control", "no-store");
  server_.sendHeader("Referrer-Policy", "no-referrer");
  server_.send(200, "application/json", body);
}

void WebServerManager::lineMaintenanceCommit() {
  // A deprecated "userId" field from older tooling is tolerated and ignored.
  if (!validPost(server_.hasArg("userId") ? 6 : 5, 2048)) {
    server_.send(400, "application/json", "{\"error\":\"invalid_request\"}"); return;
  }
  if (!lineOwnerAuthorized()) { clearLineMaintenance(); return; }

  char ownerToken[65] = {}, nonce[33] = {};
  const bool ownerFields = copyArg("token", ownerToken, sizeof(ownerToken)) &&
      copyArg("nonce", nonce, sizeof(nonce));
  const uint32_t now = millis();
  const bool locked = lock_ && lock_->isLocked() && !lock_->remainingUnlockMs(now);
  const bool sameToken = ownerFields &&
      memcmp(ownerToken, lineMaintenanceOwnerToken_, sizeof(ownerToken)) == 0;
  const bool ready = sameToken && lineMaintenanceNonce_[0] && locked &&
      uint32_t(now - lineMaintenanceStarted_) < 120000 &&
      lineMaintenancePinRevision_ == AdminPin::revision();
  memset(ownerToken, 0, sizeof(ownerToken));
  memset(nonce, 0, sizeof(nonce));
  if (!ready) {
    if (lineMaintenanceNonce_[0] &&
        (uint32_t(now - lineMaintenanceStarted_) >= 120000 ||
         lineMaintenancePinRevision_ != AdminPin::revision())) clearLineMaintenance();
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(403, "application/json", "{\"error\":\"maintenance_unavailable\"}");
    return;
  }

  char submittedNonce[33] = {}, ownerAgain[65] = {};
  const bool nonceFields = copyArg("token", ownerAgain, sizeof(ownerAgain)) &&
      copyArg("nonce", submittedNonce, sizeof(submittedNonce));
  uint8_t difference = 0;
  for (size_t i = 0; i < sizeof(lineMaintenanceNonce_); ++i)
    difference |= (uint8_t)(lineMaintenanceNonce_[i] ^ submittedNonce[i]);
  const bool nonceValid = nonceFields && difference == 0;
  memset(submittedNonce, 0, sizeof(submittedNonce));
  memset(ownerAgain, 0, sizeof(ownerAgain));
  if (!nonceValid) {
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(403, "application/json", "{\"error\":\"maintenance_unavailable\"}");
    return;
  }

  // A submitted valid capability is consumed before parsing or storage work.
  clearLineMaintenance();
  char label[65] = {}, lineToken[513] = {}, basicId[65] = {};
  const bool basicIdField = server_.hasArg("publicBasicId") &&
      (server_.arg("publicBasicId").length() == 0 ||
       copyArg("publicBasicId", basicId, sizeof(basicId)));
  const bool fields = copyArg("label", label, sizeof(label)) &&
      copyArg("lineToken", lineToken, sizeof(lineToken)) &&
      basicIdField;
  const LineNotifications::Result result = fields
      ? LineNotifications::save(label, lineToken, basicId)
      : LineNotifications::Result::InvalidInput;
  memset(label, 0, sizeof(label));
  memset(lineToken, 0, sizeof(lineToken));
  memset(basicId, 0, sizeof(basicId));
  sendLineResult(server_, result);
}

void WebServerManager::lineMaintenanceCancel() {
  if (!validPost(1, 256)) {
    server_.send(400, "application/json", "{\"error\":\"invalid_request\"}"); return;
  }
  if (!lineOwnerAuthorized()) { clearLineMaintenance(); return; }
  clearLineMaintenance();
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "application/json", "{\"ok\":true}");
}

void WebServerManager::managementSummary() {
  if (!validPost(1)) { server_.send(400, "application/json", "{\"error\":\"invalid_request\"}"); return; }
  if (!managementAuthorized()) return;
  if (!lock_) { server_.send(503, "application/json", "{\"error\":\"unavailable\"}"); return; }
  size_t users = 0, devices = 0, active = 0, revoked = 0;
  const bool healthy = enrollment_->databaseIntegrity(users, devices, active, revoked);
  // Only non-secret live state. This handler has no lock or storage mutation.
  String json = "{\"locked\":";
  json += lock_->isLocked() ? "true" : "false";
  json += ",\"uptimeMs\":"; json += String(millis());
  json += ",\"freeHeap\":"; json += String(ESP.getFreeHeap());
  json += ",\"minFreeHeap\":"; json += String(ESP.getMinFreeHeap());
  json += ",\"databaseHealthy\":"; json += healthy ? "true" : "false";
  json += ",\"identities\":"; json += String((unsigned)users);
  json += ",\"owners\":"; json += String((unsigned)devices);
  json += ",\"activeIdentities\":"; json += String((unsigned)active);
  json += ",\"revokedIdentities\":"; json += String((unsigned)revoked);
  json += ",\"canonicalHost\":"; appendJson(json, CanonicalOrigin::host());
  json += ",\"firmware\":\"Single identity checkpoint\"}";
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "application/json", json);
}

void WebServerManager::managementAddUser() {
  server_.send(410, "application/json", "{\"error\":\"retired\"}");
}

void WebServerManager::managementEnroll() {
  if (!validPost(3)) { server_.send(400, "application/json", "{\"error\":\"invalid_request\"}"); return; }
  if (!managementAuthorized()) return;
  if (!requireEnrollmentLan()) return;
  char role[8] = {}, name[128] = {}, url[128] = {};
  if (!copyArg("role", role, sizeof(role)) ||
      !copyArg("name", name, sizeof(name)) ||
      !enrollment_->createEnrollment(name, role, millis(), url)) {
    server_.send(enrollment_->nameTaken() ? 409 : 400, "application/json",
        enrollment_->nameTaken() ? "{\"error\":\"name_taken\"}" : "{\"error\":\"invalid_or_storage\"}"); return;
  }
  String svg;
  if (!enrollmentSvg(url, svg)) { server_.send(503,"application/json","{\"error\":\"qr_unavailable\"}"); return; }
  String body = "{\"ok\":true,\"url\":\""; body += url; body += "\",\"expiresIn\":120,\"qrSvg\":";
  appendJson(body, svg.c_str()); body += '}';
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "application/json", body);
  memset(url, 0, sizeof(url));
}

void WebServerManager::managementRevoke() {
  if (!validPost(2)) { server_.send(400, "application/json", "{\"error\":\"invalid_request\"}"); return; }
  if (!managementAuthorized()) return;
  char id[9] = {};
  if (!copyArg("deviceId", id, sizeof(id)) || !enrollment_->revoke(id)) {
    server_.send(400, "application/json", "{\"error\":\"invalid_or_storage\"}"); return;
  }
  server_.send(200, "application/json", "{\"ok\":true}");
}

void WebServerManager::managementUserStatus() {
  server_.send(410, "application/json", "{\"error\":\"retired\"}");
}

void WebServerManager::enrollmentCheck() {
  if (!requireEnrollmentLan()) return;
  char session[65] = {};
  String json;
  const bool valid = configured_ && enrollment_ &&
      copyArg("session", session, sizeof(session)) &&
      enrollment_->enrollmentInfo(session, millis(), json);
  memset(session, 0, sizeof(session));
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(valid ? 200 : 403, "application/json",
               valid ? json : "{\"error\":\"expired_or_used\"}");
}

void WebServerManager::enrollmentCredentialStatus() {
  if(!sensitiveRateLimit())return;
  if(!validPost(3)){server_.send(400,"application/json","{\"error\":\"invalid_request\"}");return;}
  if(!requireEnrollmentLan())return;
  char session[65]={},id[9]={},credential[65]={};
  const char* state=nullptr;
  if(configured_&&enrollment_&&copyArg("session",session,sizeof(session))&&
     copyArg("deviceId",id,sizeof(id))&&copyArg("credential",credential,sizeof(credential)))
    state=enrollment_->credentialStatus(session,id,credential,millis());
  memset(session,0,sizeof(session));memset(id,0,sizeof(id));
  volatile char* erase=credential;for(size_t i=0;i<sizeof(credential);++i)erase[i]=0;
  server_.sendHeader("Cache-Control","no-store");
  if(!state){server_.send(403,"application/json","{\"error\":\"unavailable\"}");return;}
  String body="{\"state\":\"";body+=state;body+="\"}";server_.send(200,"application/json",body);
}

void WebServerManager::enrollmentComplete() {
  if (!sensitiveRateLimit()) return;
  if (!validPost(2)) { server_.send(400, "application/json", "{\"error\":\"invalid_request\"}"); return; }
  if (!requireEnrollmentLan()) return;
  char session[65] = {}, credential[65] = {}, id[9] = {};
  const bool valid = copyArg("session", session, sizeof(session)) &&
      copyArg("credential", credential, sizeof(credential)) &&
      enrollment_->completeEnrollment(session, credential, millis(), id);
  memset(session, 0, sizeof(session)); memset(credential, 0, sizeof(credential));
  if (!valid) { server_.send(403, "application/json", "{\"error\":\"expired_invalid_or_storage\"}"); return; }
  String body = "{\"ok\":true,\"deviceId\":\""; body += id; body += "\"}";
  server_.send(200, "application/json", body);
}

void WebServerManager::networkStatus() {
  if (!validPost(1)) { server_.send(400, "application/json", "{\"error\":\"invalid_request\"}"); return; }
  if (!managementAuthorized()) return;
  if (!network_) { server_.send(503, "application/json", "{\"error\":\"network\"}"); return; }
  String body = "{\"mode\":\"";
  body += WiFi.getMode() == WIFI_STA ? "STA" : WiFi.getMode() == WIFI_AP_STA ? "AP_STA" : "AP";
  body += "\",\"apSsid\":"; appendJson(body, network_->ssid());
  body += ",\"apIp\":"; appendJson(body, network_->ip().toString().c_str());
  body += ",\"recovery\":"; body += network_->recoveryActive() ? "true" : "false";
  body += ",\"apEnabled\":"; body += network_->apEnabled() ? "true" : "false";
  body += ",\"policy\":\""; body += network_->staIp() != IPAddress(0,0,0,0) ? "home_lan" : "setup_or_recovery"; body += '"';
  body += ",\"staState\":\"";
  switch (network_->staState()) {
    case NetworkManager::StaState::Connecting: body += "connecting"; break;
    case NetworkManager::StaState::Connected: body += "connected"; break;
    case NetworkManager::StaState::Failed: body += "failed"; break;
    default: body += "disconnected"; break;
  }
  body += "\",\"staSsid\":"; appendJson(body, network_->staSsid());
  body += ",\"staIp\":"; appendJson(body, network_->staIp().toString().c_str());
  body += ",\"rssi\":"; body += String(network_->staRssi());
  body += ",\"canonicalHost\":"; appendJson(body, CanonicalOrigin::host());
  body += ",\"canonicalOrigin\":\"http://"; body += CanonicalOrigin::host(); body += "\"}";
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "application/json", body);
}

void WebServerManager::networkScan() {
  if (!validPost(1)) { server_.send(400, "application/json", "{\"error\":\"invalid_request\"}"); return; }
  if (!managementAuthorized()) return;
  if (!network_) { server_.send(503, "application/json", "{\"error\":\"network\"}"); return; }
  int count = network_->scanComplete();
  if (count == WIFI_SCAN_FAILED) {
    if (!network_->beginScan()) {
      server_.send(503, "application/json", "{\"error\":\"scan_unavailable\"}"); return;
    }
    count = network_->scanComplete();
  }
  if (count == WIFI_SCAN_RUNNING) {
    server_.send(200, "application/json", "{\"state\":\"scanning\"}"); return;
  }
  if (count < 0) { server_.send(503, "application/json", "{\"error\":\"scan_failed\"}"); return; }
  String body = "{\"state\":\"complete\",\"networks\":[";
  const int limited = count > 20 ? 20 : count;
  for (int i = 0; i < limited; ++i) {
    if (i) body += ',';
    body += "{\"ssid\":"; appendJson(body, network_->scannedSsid(i));
    body += ",\"rssi\":"; body += String(network_->scannedRssi(i));
    body += ",\"secure\":"; body += network_->scannedSecure(i) ? "true" : "false";
    body += '}';
  }
  body += "]}";
  network_->clearScan();
  server_.send(200, "application/json", body);
}

void WebServerManager::networkConnect() {
  if (!validPost(3)) { server_.send(400, "application/json", "{\"error\":\"invalid_request\"}"); return; }
  if (!managementAuthorized()) return;
  if (!network_) { server_.send(503, "application/json", "{\"error\":\"network\"}"); return; }
  char ssid[33] = {}, password[65] = {};
  bool fields = copyArg("ssid", ssid, sizeof(ssid)) && server_.hasArg("password");
  if (fields) {
    const String value = server_.arg("password");
    fields = value.length() < sizeof(password) &&
             memchr(value.c_str(), '\0', value.length()) == nullptr;
    if (fields) memcpy(password, value.c_str(), value.length());
  }
  const bool started = fields && network_->startCandidateSta(ssid, password, millis());
  memset(password, 0, sizeof(password));
  if (!started) {
    server_.send(400, "application/json", "{\"error\":\"invalid_or_busy\"}"); return;
  }
  server_.send(200, "application/json", "{\"ok\":true,\"state\":\"connecting\"}");
}

void WebServerManager::handoffCreate() {
  server_.send(410, "application/json", "{\"error\":\"retired\"}");
}

void WebServerManager::handoffComplete() {
  server_.send(410, "application/json", "{\"error\":\"retired\"}");
}

void WebServerManager::bootstrapCreate() {
  server_.send(410, "application/json", "{\"error\":\"retired\"}");
}

void WebServerManager::bootstrapComplete() {
  server_.send(410, "application/json", "{\"error\":\"retired\"}");
}

void WebServerManager::ownerVerify() {
  server_.send(410, "application/json", "{\"error\":\"retired\"}");
}

void WebServerManager::accessRequest() {
  if (!sensitiveRateLimit()) return;
  if (!validPost(3) || !access_) {
    server_.send(400, "application/json", "{\"error\":\"invalid_request\"}"); return;
  }
  char session[65] = {}, deviceId[9] = {}, credential[65] = {};
  const bool fields = copyArg("session", session, sizeof(session)) &&
      copyArg("deviceId", deviceId, sizeof(deviceId)) &&
      copyArg("credential", credential, sizeof(credential));
  uint32_t durationMs = 0;
  const bool homeLan=homeLanRequest();
  const auto result = fields ? access_->request(session, deviceId, credential,
                                                  millis(), durationMs, homeLan)
      : AccessController::Result::Denied;
    memset(session, 0, sizeof(session));
    memset(credential, 0, sizeof(credential));
    if (result == AccessController::Result::Unlocked) {
    char body[80];
    snprintf(body, sizeof(body), "{\"ok\":true,\"unlockSeconds\":%u}",
             (unsigned)(durationMs / 1000));
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "application/json", body);
    Serial.println("ACCESS: UNLOCKED");
  } else if (result == AccessController::Result::AlreadyUnlocked) {
    server_.send(409, "application/json", "{\"error\":\"already_unlocked\"}");
  } else if (result == AccessController::Result::StorageFault) {
    server_.send(503, "application/json", "{\"error\":\"storage_unavailable\"}");
  } else {
    server_.send(403, "application/json", "{\"error\":\"access_denied\"}");
  }
}

void WebServerManager::factoryResetConfirm() {
  if (!configured_ || !reset_ || !validPost(3)) {
    server_.send(400, "application/json", "{\"error\":\"invalid_request\"}");
    return;
  }
  char session[65] = {}, deviceId[9] = {}, credential[65] = {};
  const bool fields = copyArg("session", session, sizeof(session)) &&
                      copyArg("deviceId", deviceId, sizeof(deviceId)) &&
                      copyArg("credential", credential, sizeof(credential));
  const auto result = fields ? reset_->request(session, deviceId, credential,
                                                millis())
      : FactoryResetController::Result::Denied;
  memset(session, 0, sizeof(session));
  memset(credential, 0, sizeof(credential));
  server_.sendHeader("Cache-Control", "no-store");
  if (result == FactoryResetController::Result::ResetScheduled) {
    server_.send(200, "application/json", "{\"ok\":true}");
    Serial.println("FACTORY RESET: COMPLETE, RESTARTING");
  } else if (result == FactoryResetController::Result::StorageFailure) {
    server_.send(503, "application/json", "{\"error\":\"storage_unavailable\"}");
    Serial.println("FACTORY RESET: STORAGE FAILURE (LOCKED)");
  } else {
    server_.send(403, "application/json", "{\"error\":\"reset_denied\"}");
  }
}

bool WebServerManager::sensitiveRateLimit() {
  const uint32_t now=millis();
  if(uint32_t(now-rateStarted_)>=10000){rateStarted_=now;rateCount_=0;}
  if(rateCount_++<8)return true;
  rateCount_=8;server_.sendHeader("Retry-After","10");
  server_.send(429,"application/json","{\"error\":\"rate_limited\"}");return false;
}
bool WebServerManager::ownerPinAuthorized() {
  if(!managementAuthorized())return false;
  char token[65]={};
  bool owner=copyArg("token",token,sizeof(token))&&enrollment_->authorizedOwner(token,millis());
  memset(token,0,sizeof(token));
  if(!owner){server_.send(403,"application/json","{\"error\":\"owner_required\"}");return false;}
  if(network_&&network_->staState()==NetworkManager::StaState::Connecting){server_.send(409,"application/json","{\"error\":\"network_transition_active\"}");return false;}
  if(!lock_||!lock_->isLocked()||lock_->remainingUnlockMs(millis())){
    server_.send(409,"application/json","{\"error\":\"unlock_active\"}");return false;
  }
  lock_->lock();
  uint32_t now=millis();if(uint32_t(now-pinRateStarted_)>=60000){pinRateStarted_=now;pinRateCount_=0;}
  if(pinRateCount_>=3){server_.sendHeader("Retry-After","60");server_.send(429,"application/json","{\"error\":\"rate_limited\"}");return false;}
  ++pinRateCount_;
  server_.sendHeader("Cache-Control","no-store");server_.sendHeader("Referrer-Policy","no-referrer");
  return true;
}
void WebServerManager::adminPinChange(){
 if(!validPost(4)){server_.send(400,"application/json","{\"error\":\"invalid_request\"}");return;}
 if(!ownerPinAuthorized())return;
 char old[5]={},next[5]={},confirm[5]={};
 bool valid=copyArg("current",old,sizeof(old))&&copyArg("next",next,sizeof(next))&&copyArg("confirm",confirm,sizeof(confirm));
 // Format/match problems of the new PIN are reported before the current PIN is
 // verified, so they never consume an attempt. A wrong current PIN still goes
 // through AdminPin::change -> verify with the shared counter and lockout.
 const char* error=nullptr;
 if(!valid||!AdminPin::valid(next)||!AdminPin::valid(confirm))error="invalid_new_pin";
 else if(strcmp(next,confirm))error="pin_mismatch";
 auto result=error?AdminPin::Result::Invalid:AdminPin::change(old,next,confirm,millis());
 volatile char* p=old;for(int i=0;i<5;++i)p[i]=0;p=next;for(int i=0;i<5;++i)p[i]=0;p=confirm;for(int i=0;i<5;++i)p[i]=0;
 if(result==AdminPin::Result::Ok){server_.send(200,"application/json","{\"changed\":true}");clearLineMaintenance();return;}
 if(error){server_.send(400,"application/json",!strcmp(error,"pin_mismatch")?"{\"error\":\"pin_mismatch\"}":"{\"error\":\"invalid_new_pin\"}");return;}
 if(result==AdminPin::Result::Locked){server_.sendHeader("Retry-After","60");server_.send(429,"application/json","{\"error\":\"pin_locked\"}");return;}
 if(result==AdminPin::Result::StorageFailure){server_.send(503,"application/json","{\"error\":\"storage_unavailable\"}");return;}
 server_.send(403,"application/json","{\"error\":\"current_pin_rejected\"}");
}
void WebServerManager::unlockDuration(){
 // Owner-only read (token) or update (token + seconds) of the single stored
 // ConfigStore::unlockDurationMs. Never touches the lock output.
 const bool update=server_.hasArg("seconds");
 if(!validPost(update?2:1)){server_.send(400,"application/json","{\"error\":\"invalid_request\"}");return;}
 if(!lineOwnerAuthorized())return;
 if(!config_||!config_->healthy()||!config_->config().configured){server_.send(503,"application/json","{\"error\":\"storage_unavailable\"}");return;}
 if(update){
   const String text=server_.arg("seconds");
   bool digits=text.length()>=1&&text.length()<=2;
   for(size_t i=0;digits&&i<text.length();++i)digits=text[i]>='0'&&text[i]<='9';
   const uint32_t seconds=digits?(uint32_t)text.toInt():0;
   if(seconds<kMinUnlockSeconds||seconds>kMaxUnlockSeconds){server_.send(400,"application/json","{\"error\":\"invalid_duration\"}");return;}
   if(!lock_||!lock_->isLocked()||lock_->remainingUnlockMs(millis())){server_.send(409,"application/json","{\"error\":\"unlock_active\"}");return;}
   SmartLockConfig next=config_->config();
   next.unlockDurationMs=seconds*1000;
   if(!config_->save(next)){server_.send(503,"application/json","{\"error\":\"storage_unavailable\"}");return;}
 }
 char body[96];
 snprintf(body,sizeof(body),"{\"unlockSeconds\":%u,\"min\":%u,\"max\":%u,\"saved\":%s}",
   (unsigned)(config_->config().unlockDurationMs/1000),(unsigned)kMinUnlockSeconds,(unsigned)kMaxUnlockSeconds,update?"true":"false");
 server_.sendHeader("Cache-Control","no-store");
 server_.send(200,"application/json",body);
}
