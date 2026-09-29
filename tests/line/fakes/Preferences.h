#pragma once
#include <stddef.h>
#include <stdint.h>
class Preferences {
 public:
  bool begin(const char* ns,bool readOnly=false);
  void end();
  size_t getBytesLength(const char* key);
  size_t getBytes(const char* key,void* dst,size_t len);
  size_t putBytes(const char* key,const void* src,size_t len);
  uint8_t getUChar(const char* key,uint8_t fallback=0);
  size_t putUChar(const char* key,uint8_t value);
  bool remove(const char* key);
 private: const char* ns_=nullptr; bool ro_=false;
};
