#pragma once

#include <stdint.h>

namespace HeapTrace {
enum class Label : uint8_t {
  PreNetwork,
  AfterApHttpStartup,
  BeforeLineBegin,
  AfterLineBegin,
  LineWorkerBeforeFirstQuota,
  LineWorkerAfterFirstQuota,
  LineWorkerBeforeFirstPush,
  LineWorkerAfterFirstPush
};

// Temporary fixed-capacity heap characterization. Captures never allocate or
// print; the records are emitted only after the fixed DIAG_HEAP command.
void capture(Label label, uint8_t lineState = 0xff, bool includeTaskStack = false);
void print();
}
