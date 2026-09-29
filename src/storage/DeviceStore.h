#pragma once

#include "RecordCodec.h"

namespace smartlock {
namespace storage {

class DeviceStore {
 public:
  static const size_t kMaxRecords = 64;

  // Calls are synchronous and currently assume a single caller/task.
  static bool load(DeviceRecord* records, size_t capacity, size_t& count);
  static bool replace(const DeviceRecord* records, size_t count);
  static bool validate();
};

}  // namespace storage
}  // namespace smartlock
