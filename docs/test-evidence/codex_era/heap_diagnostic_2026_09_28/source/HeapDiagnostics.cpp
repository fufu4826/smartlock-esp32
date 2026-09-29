#include "HeapDiagnostics.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp32-hal-psram.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../notifications/LineNotifications.h"

namespace {
constexpr uint8_t kSnapshotCapacity = 24;
constexpr uint8_t kPhaseCount = 23;
constexpr uint8_t kIntegrityUnknown = 0;
constexpr uint8_t kIntegrityPass = 1;
constexpr uint8_t kIntegrityFail = 2;

struct CapabilityStats {
  uint32_t totalFree;
  uint32_t totalAllocated;
  uint32_t minimumFree;
  uint32_t largestFree;
  uint32_t freeBlocks;
  uint32_t allocatedBlocks;
};

struct Snapshot {
  uint32_t sequence;
  uint32_t timestampMs;
  uint8_t phase;
  uint8_t psramPresent;
  uint8_t integrity;
  uint8_t reserved;
  CapabilityStats internal;
  CapabilityStats eightBit;
  CapabilityStats internalEightBit;
  CapabilityStats internalDma;
  uint32_t psramTotal;
  uint32_t psramFree;
  uint32_t loopStackHwm;
  uint32_t lineStackHwm;
};

static_assert(sizeof(CapabilityStats) == 24, "heap diagnostic capability record changed");
static_assert(sizeof(Snapshot) == 124, "heap diagnostic snapshot size changed");
static_assert(sizeof(Snapshot) * kSnapshotCapacity <= 3072,
              "heap diagnostic BSS budget exceeded");

Snapshot gSnapshots[kSnapshotCapacity] = {};
uint8_t gSnapshotCount = 0;
bool gSeen[kPhaseCount] = {};
bool gStableRegionSummaryPrinted = false;
bool gQuotaRequestActive = false;
uint32_t gBootTimestampMs = 0;
TaskHandle_t gLoopTask = nullptr;
portMUX_TYPE gSnapshotMux = portMUX_INITIALIZER_UNLOCKED;

const char* phaseName(uint8_t phase) {
  static const char* const names[kPhaseCount] = {
      "H00", "H01", "H02", "H03", "H04", "H05", "H06", "H07",
      "H08", "H09", "H10", "H11", "H12", "H13", "H14", "H15",
      "H16", "H17", "H18", "H19", "H20", "H21", "H22"};
  return phase < kPhaseCount ? names[phase] : "INVALID";
}

CapabilityStats readStats(uint32_t capabilities) {
  multi_heap_info_t info = {};
  heap_caps_get_info(&info, capabilities);
  return {info.total_free_bytes, info.total_allocated_bytes,
          info.minimum_free_bytes, info.largest_free_block,
          info.free_blocks, info.allocated_blocks};
}

void printStats(const char* name, const CapabilityStats& stats) {
  Serial.printf(" %s=%lu/%lu/%lu/%lu/%lu/%lu", name,
                static_cast<unsigned long>(stats.totalFree),
                static_cast<unsigned long>(stats.totalAllocated),
                static_cast<unsigned long>(stats.minimumFree),
                static_cast<unsigned long>(stats.largestFree),
                static_cast<unsigned long>(stats.freeBlocks),
                static_cast<unsigned long>(stats.allocatedBlocks));
}
}  // namespace

namespace HeapDiagnostics {

void capture(Phase phase, bool workerContext) {
  const uint8_t phaseId = static_cast<uint8_t>(phase);
  if (phaseId >= kPhaseCount) return;

  if (!gLoopTask && !workerContext) gLoopTask = xTaskGetCurrentTaskHandle();
  Snapshot snapshot = {};
  snapshot.timestampMs = millis();
  snapshot.phase = phaseId;
  snapshot.psramPresent = psramFound() ? 1 : 0;
  snapshot.internal = readStats(MALLOC_CAP_INTERNAL);
  snapshot.eightBit = readStats(MALLOC_CAP_8BIT);
  snapshot.internalEightBit = readStats(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  snapshot.internalDma = readStats(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  snapshot.psramTotal = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  snapshot.psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  snapshot.loopStackHwm = gLoopTask ? uxTaskGetStackHighWaterMark(gLoopTask) : 0;
  snapshot.lineStackHwm = workerContext ? uxTaskGetStackHighWaterMark(nullptr)
                                        : LineNotifications::stackHighWaterMark();

  portENTER_CRITICAL(&gSnapshotMux);
  if (gSnapshotCount < kSnapshotCapacity && !gSeen[phaseId]) {
    snapshot.sequence = static_cast<uint32_t>(gSnapshotCount) + 1;
    gSnapshots[gSnapshotCount++] = snapshot;
    gSeen[phaseId] = true;
    if (phase == Phase::H00) gBootTimestampMs = snapshot.timestampMs;
  }
  portEXIT_CRITICAL(&gSnapshotMux);

  if (phase == Phase::H22) {
    const bool intact = heap_caps_check_integrity(
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT, false);
    portENTER_CRITICAL(&gSnapshotMux);
    for (uint8_t i = 0; i < gSnapshotCount; ++i) {
      if (gSnapshots[i].phase == phaseId) {
        gSnapshots[i].integrity = intact ? kIntegrityPass : kIntegrityFail;
        break;
      }
    }
    portEXIT_CRITICAL(&gSnapshotMux);
  }
}

void captureStableIdleIfReady() {
  bool ready = false;
  portENTER_CRITICAL(&gSnapshotMux);
  ready = gSeen[static_cast<uint8_t>(Phase::H00)] &&
      !gSeen[static_cast<uint8_t>(Phase::H22)] && !gQuotaRequestActive &&
      static_cast<uint32_t>(millis() - gBootTimestampMs) >= 45000UL;
  portEXIT_CRITICAL(&gSnapshotMux);
  if (ready) capture(Phase::H22);
}

void setQuotaRequestActive(bool active) {
  portENTER_CRITICAL(&gSnapshotMux);
  gQuotaRequestActive = active;
  portEXIT_CRITICAL(&gSnapshotMux);
}

void print() {
  uint8_t count = 0;
  bool seen[kPhaseCount] = {};
  bool printRegions = false;
  portENTER_CRITICAL(&gSnapshotMux);
  count = gSnapshotCount;
  for (uint8_t i = 0; i < kPhaseCount; ++i) seen[i] = gSeen[i];
  const bool quotaActive = gQuotaRequestActive;
  printRegions = gSeen[static_cast<uint8_t>(Phase::H22)] &&
      !gStableRegionSummaryPrinted && !quotaActive;
  if (printRegions) gStableRegionSummaryPrinted = true;
  portEXIT_CRITICAL(&gSnapshotMux);

  Serial.println("DIAG_HEAP BEGIN (free/allocated/minimum/largest/free_blocks/allocated_blocks)");
  for (uint8_t i = 0; i < count; ++i) {
    Snapshot s = {};
    portENTER_CRITICAL(&gSnapshotMux);
    s = gSnapshots[i];
    portEXIT_CRITICAL(&gSnapshotMux);
    Serial.printf("DIAG_HEAP seq=%lu phase=%s ms=%lu psram=%u psram_total=%lu psram_free=%lu loop_hwm_bytes=%lu line_hwm_bytes=%lu integrity=%s",
        static_cast<unsigned long>(s.sequence), phaseName(s.phase),
        static_cast<unsigned long>(s.timestampMs), s.psramPresent,
        static_cast<unsigned long>(s.psramTotal), static_cast<unsigned long>(s.psramFree),
        static_cast<unsigned long>(s.loopStackHwm), static_cast<unsigned long>(s.lineStackHwm),
        s.integrity == kIntegrityPass ? "PASS" :
            (s.integrity == kIntegrityFail ? "FAIL" : "N/A"));
    printStats("INTERNAL", s.internal);
    printStats("8BIT", s.eightBit);
    printStats("INTERNAL_8BIT", s.internalEightBit);
    printStats("INTERNAL_DMA", s.internalDma);
    Serial.println();
    if (s.phase == static_cast<uint8_t>(Phase::H22)) {
      Serial.printf("DIAG_HEAP H22_HEAP_INTEGRITY=%s\n",
                    s.integrity == kIntegrityPass ? "PASS" :
                        (s.integrity == kIntegrityFail ? "FAIL" : "UNAVAILABLE"));
    }
  }

  for (uint8_t phase = 0; phase < kPhaseCount; ++phase) {
    if (!seen[phase]) Serial.printf("DIAG_HEAP phase=%s unavailable\n", phaseName(phase));
  }
  if (quotaActive) Serial.println("DIAG_HEAP NOTE quota TLS request was active during command");
  if (printRegions) {
    Serial.println("DIAG_HEAP REGION SUMMARY INTERNAL|8BIT");
    heap_caps_print_heap_info(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }
  Serial.println("DIAG_HEAP END");
}

}  // namespace HeapDiagnostics
