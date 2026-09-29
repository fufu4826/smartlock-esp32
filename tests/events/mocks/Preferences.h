#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>

class Preferences {
 public:
  bool begin(const char* ns, bool) { ns_ = ns ? ns : ""; open_ = true; return true; }
  void end() { open_ = false; }
  bool clear() {
    if (!open_) return false;
    const std::string prefix = ns_ + "/";
    for (auto it = values().begin(); it != values().end();) {
      if (it->first.compare(0, prefix.size(), prefix) == 0) it = values().erase(it);
      else ++it;
    }
    return true;
  }
  uint32_t getULong(const char* key, uint32_t fallback = 0) const {
    auto it = values().find(ns_ + "/" + (key ? key : ""));
    if (it == values().end() || it->second.size() != sizeof(uint32_t)) return fallback;
    uint32_t result = 0; for (size_t i=0;i<sizeof(result);++i) result |= static_cast<uint32_t>(static_cast<uint8_t>(it->second[i])) << (8*i);
    return result;
  }
  size_t putULong(const char* key, uint32_t value) {
    if (!open_) return 0;
    std::string bytes(sizeof(value), '\0'); for (size_t i=0;i<sizeof(value);++i) bytes[i]=static_cast<char>(value>>(8*i));
    values()[ns_ + "/" + (key ? key : "")] = bytes; return sizeof(value);
  }
  size_t getBytesLength(const char* key) const {
    auto it = values().find(ns_ + "/" + (key ? key : "")); return it == values().end() ? 0 : it->second.size();
  }
  size_t getBytes(const char* key, void* output, size_t length) const {
    auto it = values().find(ns_ + "/" + (key ? key : ""));
    if (it == values().end() || !output) return 0;
    const size_t copied = length < it->second.size() ? length : it->second.size();
    std::memcpy(output, it->second.data(), copied); return copied;
  }
  size_t putBytes(const char* key, const void* input, size_t length) {
    if (!open_ || !input) return 0;
    ++writeCount();
    values()[ns_ + "/" + (key ? key : "")] = std::string(static_cast<const char*>(input), length);
    return length;
  }
  static void clearAll() { values().clear(); writeCount() = 0; }
  static size_t writes() { return writeCount(); }
 private:
  static size_t& writeCount() { static size_t count = 0; return count; }
  static std::map<std::string,std::string>& values() { static std::map<std::string,std::string> v; return v; }
  std::string ns_; bool open_ = false;
};
