#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
extern uint32_t mockMillis;
inline uint32_t millis() { return mockMillis; }
class SerialClass {
 public:
  int available() { return 0; }
  int read() { return -1; }
  template<class... T> int printf(const char*, T...) { return 0; }
  size_t write(const uint8_t*, size_t n) { return n; }
  size_t println(const char*) { return 0; }
};
extern SerialClass Serial;
class ESPClass { public: uint32_t getFreeHeap() const { return 100000; } uint32_t getMinFreeHeap() const { return 90000; } uint64_t getEfuseMac() const { return 0x1122334455667788ULL; } };
extern ESPClass ESP;
