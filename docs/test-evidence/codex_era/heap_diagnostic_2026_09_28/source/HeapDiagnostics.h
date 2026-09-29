#pragma once

#include <stdint.h>

namespace HeapDiagnostics {

enum class Phase : uint8_t {
  H00, H01, H02, H03, H04, H05, H06, H07, H08, H09, H10, H11,
  H12, H13, H14, H15, H16, H17, H18, H19, H20, H21, H22
};

void capture(Phase phase, bool workerContext = false);
void captureStableIdleIfReady();
void setQuotaRequestActive(bool active);
void print();

}  // namespace HeapDiagnostics
