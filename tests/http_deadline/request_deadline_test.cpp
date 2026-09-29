#include "detail/RequestDeadline.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

namespace { uint32_t nowMs = 0; }
uint32_t millis() { return nowMs; }
void delay(uint32_t ms) { nowMs += ms; }

class FakeSlowClient {
 public:
  FakeSlowClient(std::string bytes, uint32_t interval = 0)
      : bytes_(std::move(bytes)), interval_(interval), lastReadAt_(nowMs) {}
  int available() {
    if (position_ >= bytes_.size() || uint32_t(nowMs - lastReadAt_) < interval_) return 0;
    return interval_ ? 1 : static_cast<int>(bytes_.size() - position_);
  }
  int read() {
    if (!available()) return -1;
    if (interval_) lastReadAt_ = nowMs;
    return static_cast<unsigned char>(bytes_[position_++]);
  }
 private:
  std::string bytes_;
  size_t position_ = 0;
  uint32_t interval_;
  uint32_t lastReadAt_;
};

static size_t readBody(FakeSlowClient& client, size_t length, uint32_t started,
                       uint32_t budget = 5000) {
  RequestDeadline deadline(started, budget);
  size_t count = 0;
  char* body = readRequestBody(client, length, count, deadline);
  free(body);
  return count;
}

int main() {
  size_t emptyLength = 0;
  FakeSlowClient empty("");
  assert(readRequestBody(empty, 0, emptyLength, RequestDeadline(nowMs)) == nullptr);
  assert(emptyLength == 0);

  // Complete bodies at both production limits are accepted when sent promptly.
  nowMs = 20;
  FakeSlowClient fast512(std::string(512, 'x'));
  assert(readBody(fast512, 512, nowMs) == 512);
  nowMs = 200;
  FakeSlowClient fastSetup(std::string(1024, 'y'));
  assert(readBody(fastSetup, 1024, nowMs) == 1024);

  // A one-byte trickle cannot renew the original request deadline.
  nowMs = 1000;
  const uint32_t trickleStarted = nowMs;
  FakeSlowClient trickle(std::string(512, 'z'), 25);
  const size_t received = readBody(trickle, 512, trickleStarted);
  assert(received < 512);
  assert(uint32_t(nowMs - trickleStarted) <= 5001);

  // A request whose headers consumed most of the budget gets only the remainder.
  nowMs = 6000;
  const uint32_t requestStarted = nowMs;
  nowMs += 4900;
  FakeSlowClient lateBody(std::string(20, 'b'), 20);
  assert(readBody(lateBody, 20, requestStarted) < 20);
  assert(uint32_t(nowMs - requestStarted) <= 5001);

  // Truncated bodies fail promptly, and the deadline arithmetic survives millis wrap.
  nowMs = 8000;
  FakeSlowClient truncated("short", 100);
  assert(readBody(truncated, 32, nowMs) == 5);
  nowMs = UINT32_MAX - 100;
  const uint32_t wrapStart = nowMs;
  FakeSlowClient wrapTrickle(std::string(300, 'w'), 50);
  assert(readBody(wrapTrickle, 300, wrapStart) < 300);
  assert(uint32_t(nowMs - wrapStart) <= 5001);

  std::cout << "HTTP request deadline helper: 11 checks passed\n";
}
