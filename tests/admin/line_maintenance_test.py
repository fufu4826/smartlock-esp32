"""Compile extracted production LINE maintenance handlers against host fakes."""
from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "src" / "network" / "WebServerManager.cpp"


def extract_function(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for end in range(brace, len(source)):
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
            if depth == 0:
                return source[start : end + 1]
    raise AssertionError(f"unterminated production function: {signature}")


SUPPORT = r'''#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

uint32_t clockMs = 1000;
uint32_t millis() { return clockMs; }
uint32_t esp_random() { static uint32_t n = 0x12345678; return ++n; }

class String {
 public:
  String() = default;
  String(const char* s) : value_(s ? s : "") {}
  String(const std::string& s) : value_(s) {}
  size_t length() const { return value_.length(); }
  const char* c_str() const { return value_.c_str(); }
  String& operator+=(const char* s) { value_ += s ? s : ""; return *this; }
  String& operator+=(char c) { value_ += c; return *this; }
  String& operator+=(const String& s) { value_ += s.value_; return *this; }
 private:
  std::string value_;
};

class WebServer {
 public:
  std::map<std::string, std::string> fields;
  long contentLength = 0;
  std::string contentType = "application/x-www-form-urlencoded";
  int responseCode = 0;
  std::string responseBody;
  int sendCount = 0;
  bool hasArg(const char* key) const { return fields.count(key) != 0; }
  String arg(const char* key) const {
    auto it = fields.find(key); return it == fields.end() ? String() : String(it->second);
  }
  int args() const { return static_cast<int>(fields.size()); }
  long clientContentLength() const { return contentLength; }
  String header(const char*) const { return String(contentType); }
  void sendHeader(const char*, const char*, bool = false) {}
  void send(int code, const char*, const char* body) {
    responseCode = code; responseBody = body ? body : ""; ++sendCount;
  }
  void send(int code, const char*, const String& body) {
    responseCode = code; responseBody = body.c_str(); ++sendCount;
  }
  void request(std::map<std::string, std::string> f, long length) {
    fields = std::move(f); contentLength = length; responseCode = sendCount = 0; responseBody.clear();
  }
};

namespace RequestPolicy {
bool formContentType(const char* value) { return value && std::string(value) == "application/x-www-form-urlencoded"; }
bool deviceId(const char*) { return true; }
}

struct EnrollmentManager {
  std::string actorRole = "Owner";
  std::string ownerToken = "owner-session";
  bool authorizedOwner(const char* token, uint32_t) { return actorRole == "Owner" && token && ownerToken == token; }
};
struct NetworkManager {
  enum class StaState { Idle, Connecting };
  StaState state = StaState::Idle;
  StaState staState() const { return state; }
};
struct LockController {
  bool locked = true;
  bool isLocked() const { return locked; }
  uint32_t remainingUnlockMs(uint32_t) const { return locked ? 0 : 1000; }
  void lock() { locked = true; }
};

namespace AdminPin {
enum class Result { Ok, Invalid, Locked };
int verifyCalls = 0;
int failureCallbacks = 0;
uint32_t pinRevision = 7;
uint8_t failures = 0;
uint32_t lockedAt = 0;
bool lockedOut = false;
Result verify(const char* pin, uint32_t) {
  ++verifyCalls;
  if (lockedOut && uint32_t(clockMs - lockedAt) < 60000) return Result::Locked;
  if (lockedOut) { lockedOut = false; failures = 0; }
  if (pin && std::strcmp(pin, "2468") == 0) return Result::Ok;
  ++failureCallbacks;
  if (++failures >= 3) { lockedOut = true; lockedAt = clockMs; return Result::Locked; }
  return Result::Invalid;
}
bool valid(const char* pin) { return pin && std::strlen(pin) == 4 && strspn(pin, "0123456789") == 4; }
uint32_t revision() { return pinRevision; }
}

namespace LineNotifications {
enum class Result { Ok, InvalidInput, NotConfigured, RateLimited, StorageError };
int saveCalls = 0;
std::string savedLabel, savedToken, savedBasicId;
Result save(const char* label, const char* token, const char* basicId) {
  ++saveCalls; savedLabel = label; savedToken = token; savedBasicId = basicId;
  return Result::Ok;
}
}

bool sessionAccepted = true;
'''


TEST_MAIN = r'''
int main() {
  WebServerManager manager;
  auto arm = [&](const char* token = "owner-session", const char* pin = "2468") {
    manager.server_.request({{"token", token}, {"pin", pin}}, 20);
    manager.lineMaintenanceArm();
  };
  // Broadcast configuration: no recipient field is required.
  auto commit = [&](const char* token, const char* nonce, const std::string& lineToken = "line-secret") {
    manager.server_.request({{"token", token}, {"nonce", nonce}, {"label", "home"},
      {"lineToken", lineToken}, {"publicBasicId", "@home"}}, 400);
    manager.lineMaintenanceCommit();
  };

  // The Owner/session gate is evaluated before PIN verification.
  sessionAccepted = false;
  arm();
  assert(AdminPin::verifyCalls == 0 && AdminPin::failureCallbacks == 0);
  assert(manager.server_.responseCode == 403);

  // One wrong PIN reaches AdminPin once, producing one failure callback.
  sessionAccepted = true;
  arm("owner-session", "9999");
  assert(AdminPin::verifyCalls == 1 && AdminPin::failureCallbacks == 1);
  assert(manager.server_.responseCode == 403 && manager.server_.sendCount == 1);
  arm("owner-session", "9999");
  assert(AdminPin::verifyCalls == 2 && AdminPin::failureCallbacks == 2);
  assert(manager.server_.responseCode == 403);
  arm("owner-session", "9999");
  assert(AdminPin::verifyCalls == 3 && AdminPin::failureCallbacks == 3);
  assert(manager.server_.responseCode == 429);
  manager.pinRateCount_ = 0;  // Simulate the route throttle window ending first.
  arm("owner-session", "9999");
  assert(AdminPin::verifyCalls == 4 && AdminPin::failureCallbacks == 3);
  assert(manager.server_.responseCode == 429);
  clockMs += 60000;  // The AdminPin verifier releases its persisted lockout.

  // A successful arm binds the capability to the Owner token and PIN revision.
  arm();
  assert(AdminPin::verifyCalls == 5 && manager.server_.responseCode == 200);
  const std::string nonce = manager.lineMaintenanceNonce_;
  assert(nonce.size() == 32 && manager.lineMaintenanceOwnerToken_[0] != 0);

  // A different Owner token and a wrong nonce never reach LINE storage.
  commit("other-owner", nonce.c_str());
  assert(LineNotifications::saveCalls == 0 && manager.server_.responseCode == 403);
  commit("owner-session", "00000000000000000000000000000000");
  assert(LineNotifications::saveCalls == 0 && manager.server_.responseCode == 403);

  for (const char* role : {"Admin", "User", "Guest"}) {
    manager.enrollment.actorRole = role;
    manager.server_.request({{"token", "owner-session"}}, 16);
    assert(!manager.lineOwnerAuthorized());
    assert(AdminPin::verifyCalls == 5 && LineNotifications::saveCalls == 0);
  }
  manager.enrollment.actorRole = "Owner";

  // Expired and PIN-revision-stale capabilities are rejected and cleared.
  clockMs += 120000;
  commit("owner-session", nonce.c_str());
  assert(LineNotifications::saveCalls == 0 && manager.lineMaintenanceNonce_[0] == 0);
  arm();
  const std::string revisionNonce = manager.lineMaintenanceNonce_;
  ++AdminPin::pinRevision;
  commit("owner-session", revisionNonce.c_str());
  assert(LineNotifications::saveCalls == 0 && manager.lineMaintenanceNonce_[0] == 0);

  // A valid capability saves once, then replay fails. Bounded copies reject oversize fields.
  clockMs += 60000;
  arm();
  assert(manager.server_.responseCode == 200);
  const std::string replayNonce = manager.lineMaintenanceNonce_;
  commit("owner-session", replayNonce.c_str());
  assert(LineNotifications::saveCalls == 1 && manager.server_.responseCode == 200);
  assert(LineNotifications::savedToken == "line-secret" && LineNotifications::savedBasicId == "@home");
  commit("owner-session", replayNonce.c_str());
  assert(LineNotifications::saveCalls == 1 && manager.server_.responseCode == 403);

  // Older tooling that still sends a deprecated userId is accepted; the value is ignored.
  clockMs += 60000;
  arm();
  assert(manager.server_.responseCode == 200);
  {
    const std::string legacyNonce = manager.lineMaintenanceNonce_;
    manager.server_.request({{"token", "owner-session"}, {"nonce", legacyNonce}, {"label", "home"},
      {"lineToken", "line-secret-2"}, {"userId", "U12345678901234567890123456789012"}, {"publicBasicId", "@home"}}, 400);
    manager.lineMaintenanceCommit();
    assert(LineNotifications::saveCalls == 2 && manager.server_.responseCode == 200);
    assert(LineNotifications::savedToken == "line-secret-2");
  }
  // Unknown extra fields are still rejected by the exact field-count envelope.
  clockMs += 60000;
  arm();
  {
    const std::string extraNonce = manager.lineMaintenanceNonce_;
    manager.server_.request({{"token", "owner-session"}, {"nonce", extraNonce}, {"label", "home"},
      {"lineToken", "x"}, {"publicBasicId", ""}, {"extra", "x"}}, 400);
    manager.lineMaintenanceCommit();
    assert(LineNotifications::saveCalls == 2 && manager.server_.responseCode == 400);
  }

  clockMs += 60000;
  arm();
  assert(manager.server_.responseCode == 200);
  const std::string boundsNonce = manager.lineMaintenanceNonce_;
  commit("owner-session", boundsNonce.c_str(), std::string(513, 'x'));
  assert(LineNotifications::saveCalls == 2 && manager.server_.responseCode == 400);
  assert(manager.lineMaintenanceNonce_[0] == 0);

  // The request envelope rejects excessive body sizes before authorization/storage.
  manager.server_.request({{"token", "owner-session"}, {"nonce", "x"}, {"label", "x"},
    {"lineToken", "x"}, {"publicBasicId", ""}}, 2049);
  manager.lineMaintenanceCommit();
  assert(LineNotifications::saveCalls == 2 && manager.server_.responseCode == 400);
  std::puts("Extracted production LINE maintenance handlers: PASS");
}
'''


def main() -> int:
    source = SOURCE.read_text(encoding="utf-8-sig")
    signatures = [
        "bool WebServerManager::copyArg(",
        "bool WebServerManager::validPost(",
        "bool WebServerManager::lineOwnerAuthorized()",
        "void WebServerManager::clearLineMaintenance()",
        "void WebServerManager::lineMaintenanceArm()",
        "void WebServerManager::lineMaintenanceCommit()",
        "bool WebServerManager::ownerPinAuthorized()",
    ]
    methods = [extract_function(source, signature) for signature in signatures]
    methods = [method.replace("WebServerManager::", "", 1) for method in methods]
    send_result = extract_function(source, "static void sendLineResult(")
    support = SUPPORT + r'''
void sendLineResult(WebServer&, LineNotifications::Result);
class WebServerManager {
 public:
  WebServer server_;
  EnrollmentManager* enrollment_ = &enrollment;
  NetworkManager* network_ = &network;
  LockController* lock_ = &lock;
  uint32_t pinRateStarted_ = 0;
  uint8_t pinRateCount_ = 0;
  char lineMaintenanceNonce_[33] = {};
  char lineMaintenanceOwnerToken_[65] = {};
  uint32_t lineMaintenanceStarted_ = 0;
  uint32_t lineMaintenancePinRevision_ = 0;
  bool configured_ = true;
  EnrollmentManager enrollment;
  NetworkManager network;
  LockController lock;
  bool managementAuthorized() {
    if (sessionAccepted) return true;
    server_.send(403, "application/json", "{\"error\":\"session_required\"}");
    return false;
  }
'''
    support += "\n".join(methods)
    support += "\n};\n"
    support += send_result.replace("static void sendLineResult(", "void sendLineResult(", 1)
    support += TEST_MAIN

    compiler_name = os.environ.get("CXX")
    compiler = shutil.which(compiler_name) if compiler_name else None
    if compiler is None:
        compiler = shutil.which("g++") or shutil.which("clang++")
    with tempfile.TemporaryDirectory(prefix="line-maintenance-test-") as temp_name:
        temp = Path(temp_name)
        cpp = temp / "line_maintenance_test.cpp"
        cpp.write_text(support, encoding="utf-8")
        output = temp / ("line_maintenance_test.exe" if os.name == "nt" else "line_maintenance_test")
        if compiler:
            command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(output)]
            subprocess.run(command, cwd=ROOT, check=True)
        else:
            sys.path.insert(0, str(ROOT / "tests" / "identity"))
            from run_identity_tests import find_msvc_environment
            msvc = find_msvc_environment()
            if not msvc:
                print("No host C++ compiler found (tried CXX, g++, clang++, and Visual Studio).", file=sys.stderr)
                return 2
            build = temp / "build.cmd"
            build.write_text(
                f'call "{msvc}" -arch=x64 -host_arch=x64\n'
                f'cl /nologo /std:c++17 /EHsc /W4 /WX /utf-8 "{cpp}" /Fe:"{output}"\n',
                encoding="utf-8",
            )
            env = os.environ.copy()
            windows = Path(os.environ.get("SystemRoot", r"C:\Windows"))
            env["PATH"] = ";".join(str(windows / suffix) for suffix in (
                "System32", "System32\\Wbem", "System32\\WindowsPowerShell\\v1.0"
            ))
            subprocess.run(["cmd.exe", "/d", "/c", str(build)], cwd=temp, env=env, check=True)
        subprocess.run([str(output)], cwd=ROOT, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
