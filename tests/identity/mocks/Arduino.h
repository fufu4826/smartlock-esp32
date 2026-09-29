#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
class String {
 public:
  String()=default;
  String(const char* s):value_(s?s:""){}
  String(unsigned long value):value_(std::to_string(value)){}
  String& operator=(const char* s){value_=s?s:"";return *this;}
  String& operator+=(const char* s){value_+=s?s:"";return *this;}
  String& operator+=(char c){value_+=c;return *this;}
  String& operator+=(const String& s){value_+=s.value_;return *this;}
  const char* c_str()const{return value_.c_str();}
  size_t length()const{return value_.size();}
 private: std::string value_;
};
class SerialClass { public: template<class... T> int printf(const char*,T...){return 0;} };
extern SerialClass Serial;
uint32_t millis();
void setFakeMillis(uint32_t);
