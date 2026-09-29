#pragma once

#include <cstdint>
#include <string>

class IPAddress {
 public:
  IPAddress() = default;
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
      : bytes_{a, b, c, d} {}
  bool operator==(const IPAddress& other) const {
    return bytes_[0] == other.bytes_[0] && bytes_[1] == other.bytes_[1] &&
           bytes_[2] == other.bytes_[2] && bytes_[3] == other.bytes_[3];
  }
  bool operator!=(const IPAddress& other) const { return !(*this == other); }
  std::string toString() const {
    return std::to_string(bytes_[0]) + "." + std::to_string(bytes_[1]) +
           "." + std::to_string(bytes_[2]) + "." + std::to_string(bytes_[3]);
  }

 private:
  uint8_t bytes_[4] = {};
};
