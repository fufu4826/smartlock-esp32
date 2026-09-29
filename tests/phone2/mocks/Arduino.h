#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>

class String {
 public:
  String() = default;
  String(const char* value) : value_(value ? value : "") {}
  String(unsigned long value) : value_(std::to_string(value)) {}
  String& operator=(const char* value) { value_ = value ? value : ""; return *this; }
  String& operator+=(char value) { value_ += value; return *this; }
  String& operator+=(const char* value) { if (value) value_ += value; return *this; }
  String& operator+=(const String& value) { value_ += value.value_; return *this; }
  const char* c_str() const { return value_.c_str(); }
  const std::string& stdString() const { return value_; }
 private:
  std::string value_;
};

uint32_t millis();
void setFakeMillis(uint32_t nowMs);
