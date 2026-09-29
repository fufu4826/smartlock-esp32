#pragma once
#include <stdint.h>
#include <cstdio>
extern uint32_t hostMillis;
inline uint32_t millis() { return hostMillis; }
struct FakeSerial {
  template<class... Args> int printf(const char* format, Args... args) {
    return std::printf(format, args...);
  }
};
extern FakeSerial Serial;
