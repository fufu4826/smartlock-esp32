#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <cstring>
#include "ConfigStore.h"
namespace ota_test { extern uint8_t snapshotMarker; }
class SecureBackup {
 public:
  struct Buffer {
    std::unique_ptr<uint8_t[]> bytes;
    std::size_t size = 0;
    bool allocate(std::size_t n) {
      bytes.reset(new uint8_t[n]{});
      size = n;
      return bool(bytes);
    }
  };
  static bool capture(const ConfigStore&, Buffer& out) {
    if (!out.allocate(8)) return false;
    std::memcpy(out.bytes.get(), "snapshot", 8);
    out.bytes[0] = ota_test::snapshotMarker;
    return true;
  }
};
