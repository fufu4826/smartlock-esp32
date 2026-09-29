#pragma once

#include <cstdint>

class EspMock {
 public:
  uint64_t getEfuseMac() const { return 0x112233445566ULL; }
};

extern EspMock ESP;
extern uint32_t fakeMillis;
inline uint32_t millis() { return fakeMillis; }
