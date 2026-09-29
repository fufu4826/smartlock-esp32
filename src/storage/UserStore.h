#pragma once

#include "RecordCodec.h"

namespace smartlock {
namespace storage {

class UserStore {
 public:
  static const size_t kMaxRecords = 64;

  // Calls are synchronous and currently assume a single caller/task.
  static bool load(UserRecord* records, size_t capacity, size_t& count);
  static bool replace(const UserRecord* records, size_t count);
  static bool validate();
};

}  // namespace storage
}  // namespace smartlock
