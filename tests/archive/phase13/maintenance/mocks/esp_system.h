#pragma once
#include <stdint.h>
uint32_t esp_random();
struct FakeEsp {
  uint64_t getEfuseMac() const { return 0x123456789abcULL; }
};
extern FakeEsp ESP;
