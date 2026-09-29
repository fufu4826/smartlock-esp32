#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

class String {
 public:
  String() = default;
  String(const char* value) : value_(value ? value : "") {}
  String& operator=(const char* value) {
    value_ = value ? value : "";
    return *this;
  }
  const char* c_str() const { return value_.c_str(); }

 private:
  std::string value_;
};

class SerialMock {
 public:
  void println(const char*) {}
  template <typename... Args>
  void printf(const char*, Args...) {}
};

class EspMock {
 public:
  uint64_t getEfuseMac() const { return 0x1234; }
};

extern SerialMock Serial;
extern EspMock ESP;
