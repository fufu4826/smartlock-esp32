#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
namespace ota_test {
extern std::string nvsPath;
extern bool failPreferenceWrite;
extern std::string trace;
bool loadNvs(std::string&);
bool saveNvs(const std::string&);
}
class Preferences {
 public:
  bool begin(const char*, bool) { return true; }
  bool isKey(const char* key) {
    std::string bytes;
    return std::string(key) == "result" && ota_test::loadNvs(bytes);
  }
  std::size_t getBytesLength(const char* key) {
    std::string bytes;
    return std::string(key) == "result" && ota_test::loadNvs(bytes) ? bytes.size() : 0;
  }
  std::size_t getBytes(const char* key, void* output, std::size_t size) {
    std::string bytes;
    if (std::string(key) != "result" || !ota_test::loadNvs(bytes) || size > bytes.size()) return 0;
    bytes.copy(static_cast<char*>(output), size);
    return size;
  }
  std::size_t putBytes(const char* key, const void* input, std::size_t size) {
    ota_test::trace += "receipt;";
    if (std::string(key) != "result" || ota_test::failPreferenceWrite) return 0;
    const std::string bytes(static_cast<const char*>(input), size);
    return ota_test::saveNvs(bytes) ? size : 0;
  }
  void end() {}
};
