#pragma once

#include <Arduino.h>
#include <stdlib.h>

// One wrap-safe deadline shared by every read made while parsing a request.
class RequestDeadline {
 public:
  explicit RequestDeadline(uint32_t started, uint32_t budgetMs = 5000)
      : started_(started), budgetMs_(budgetMs) {}
  bool expired(uint32_t now) const { return uint32_t(now - started_) >= budgetMs_; }
  bool expired() const { return expired(millis()); }
  uint32_t remaining(uint32_t now) const {
    const uint32_t elapsed = uint32_t(now - started_);
    return elapsed >= budgetMs_ ? 0 : budgetMs_ - elapsed;
  }
 private:
  uint32_t started_;
  uint32_t budgetMs_;
};

// Called by the production parser and host tests. It never waits past the
// request deadline and allocates at most the already-validated body limit.
template <typename Client>
char* readRequestBody(Client& client, size_t maxLength, size_t& dataLength,
                      const RequestDeadline& deadline) {
  dataLength = 0;
  if (!maxLength) return nullptr;
  char* buffer = static_cast<char*>(malloc(maxLength + 1));
  if (!buffer) return nullptr;
  buffer[0] = '\0';
  while (dataLength < maxLength && !deadline.expired()) {
    int available = client.available();
    if (available <= 0) {
      delay(1);
      continue;
    }
    size_t count = static_cast<size_t>(available);
    if (count > maxLength - dataLength) count = maxLength - dataLength;
    size_t read = 0;
    while (read < count && !deadline.expired()) {
      const int value = client.read();
      if (value < 0) break;
      buffer[dataLength + read++] = static_cast<char>(value);
    }
    dataLength += read;
    if (read != count) break;
    buffer[dataLength] = '\0';
  }
  return buffer;
}
