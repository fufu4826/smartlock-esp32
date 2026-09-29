#include <cstdlib>
#include <cstring>
#include <iostream>

#include "CanonicalOrigin.h"
#include "ESPmDNS.h"

EspMock ESP;
uint32_t fakeMillis = 0;
MDNSMock MDNS;

namespace {
int beginCalls = 0;
int endCalls = 0;
int serviceCalls = 0;
int serviceTxtCalls = 0;
bool failBegin = false;
bool failService = false;
bool failServiceTxt = false;
char lastHost[32] = {};
char lastService[16] = {};
char lastProto[16] = {};
uint16_t lastPort = 0;
int checks = 0;

void check(bool condition, const char* message) {
  ++checks;
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
}  // namespace

bool MDNSMock::begin(const char* name) {
  ++beginCalls;
  std::strncpy(lastHost, name, sizeof(lastHost) - 1);
  return !failBegin;
}

void MDNSMock::end() { ++endCalls; }

bool MDNSMock::addService(const char* service, const char* proto,
                          uint16_t port) {
  ++serviceCalls;
  std::strncpy(lastService, service, sizeof(lastService) - 1);
  std::strncpy(lastProto, proto, sizeof(lastProto) - 1);
  lastPort = port;
  return !failService;
}

bool MDNSMock::addServiceTxt(char* service, char* proto, char* key,
                             char* value) {
  ++serviceTxtCalls;
  std::strncpy(lastService, service, sizeof(lastService) - 1);
  std::strncpy(lastProto, proto, sizeof(lastProto) - 1);
  return !failServiceTxt && std::strcmp(key, "path") == 0 &&
         std::strcmp(value, "/") == 0;
}

int main() {
  const char* stableHost = CanonicalOrigin::host();
  check(std::strcmp(stableHost, "smartlock-112233445566.local") == 0,
        "canonical .local hostname remains stable");
  check(std::strcmp(CanonicalOrigin::label(), "smartlock-112233445566") == 0,
        "mDNS label remains the existing hostname");

  failBegin = true;
  check(!CanonicalOrigin::begin(), "initial mDNS failure is reported");
  check(!CanonicalOrigin::ready(), "failed initial start is not ready");
  check(beginCalls == 1 && endCalls == 1,
        "failed begin is cleaned up exactly once");

  const IPAddress noIp;
  const IPAddress ap(192, 168, 4, 1);
  CanonicalOrigin::update(1000, noIp, ap);
  CanonicalOrigin::update(4999, noIp, ap);
  check(beginCalls == 1, "failed start is not retried before five seconds");
  failBegin = false;
  CanonicalOrigin::update(5000, noIp, ap);
  check(CanonicalOrigin::ready(), "failed start recovers on scheduled retry");
  check(beginCalls == 2 && serviceCalls == 1,
        "retry initializes one HTTP service");
  check(std::strcmp(lastHost, "smartlock-112233445566") == 0 &&
            std::strcmp(lastService, "http") == 0 &&
            std::strcmp(lastProto, "tcp") == 0 && lastPort == 80,
        "retry preserves canonical hostname and fixed HTTP service");

  CanonicalOrigin::update(6000, noIp, ap);
  check(beginCalls == 2, "unchanged interfaces do not trigger a restart");

  const IPAddress sta(192, 168, 1, 179);
  CanonicalOrigin::update(7000, sta, ap);
  CanonicalOrigin::update(8499, sta, ap);
  check(beginCalls == 2, "STA address change waits for settling");
  CanonicalOrigin::update(8500, sta, ap);
  check(beginCalls == 2 && endCalls == 1 && serviceTxtCalls == 0 &&
            CanonicalOrigin::ready(),
        "settled STA address change uses IDF interface tracking without allocations");

  CanonicalOrigin::update(9000, sta, noIp);
  CanonicalOrigin::update(9500, sta, ap);
  CanonicalOrigin::update(10999, sta, ap);
  check(serviceTxtCalls == 0, "a second address change restarts its settle timer");
  CanonicalOrigin::update(11000, sta, ap);
  check(serviceTxtCalls == 0 && beginCalls == 2,
        "latest settled interface state uses IDF without restarting mDNS");

  const IPAddress noAddress;
  CanonicalOrigin::update(12000, noAddress, noAddress);
  CanonicalOrigin::update(13499, noAddress, noAddress);
  check(CanonicalOrigin::ready(), "link loss also waits for settling");
  CanonicalOrigin::update(13500, noAddress, noAddress);
  check(CanonicalOrigin::ready() && endCalls == 1,
        "link loss preserves the responder for IDF interface tracking");

  failServiceTxt = true;
  CanonicalOrigin::update(14000, noAddress, ap);
  CanonicalOrigin::update(15500, noAddress, ap);
  check(beginCalls == 2 && CanonicalOrigin::ready() && serviceTxtCalls == 0,
        "TXT API is not needed for interface tracking");
  failServiceTxt = false;
  CanonicalOrigin::update(16000, sta, ap);
  CanonicalOrigin::update(17500, sta, ap);
  check(serviceTxtCalls == 0,
        "interface changes do not bypass initialization retry rate limit");
  CanonicalOrigin::update(20499, sta, ap);
  check(serviceTxtCalls == 0, "service failure retry is rate limited");
  CanonicalOrigin::update(20500, sta, ap);
  check(beginCalls == 2 && serviceTxtCalls == 0 && CanonicalOrigin::ready(),
        "healthy interface tracking requires no repeated allocation");

  std::cout << "PASS: " << checks << " canonical mDNS lifecycle assertions\n";
}
