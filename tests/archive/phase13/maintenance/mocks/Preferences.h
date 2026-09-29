#pragma once
#include <stddef.h>
#include <string>
#include <map>

namespace FakePreferences {
void clear();
void failPutAt(size_t operation, bool afterCommit);
void disableFault();
size_t putCount();
bool corrupt(const char* name, size_t offset);
}

class Preferences {
 public:
  bool begin(const char* name, bool readOnly);
  bool isKey(const char* key) const;
  size_t getBytesLength(const char* key) const;
  size_t getBytes(const char* key, void* output, size_t length) const;
  size_t putBytes(const char* key, const void* input, size_t length);
  bool clear();
  void end();
 private:
  std::string name_;
  bool open_ = false;
  bool readOnly_ = false;
};
