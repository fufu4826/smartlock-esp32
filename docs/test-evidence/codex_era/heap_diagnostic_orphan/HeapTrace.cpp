#include "HeapTrace.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
constexpr uint8_t kCapacity = 12;
struct Snapshot {
  uint32_t atMs;
  uint32_t freeBytes;
  uint32_t totalBytes;
  uint32_t minimumFreeBytes;
  uint32_t largestFreeBlock;
  uint32_t allocatedBytes;
  uint32_t freeBlocks;
  uint32_t allocatedBlocks;
  uint32_t totalBlocks;
  uint32_t stackHighWaterBytes;
  uint8_t label;
  uint8_t lineState;
  uint8_t hasTaskStack;
};
Snapshot snapshots[kCapacity] = {};
uint8_t snapshotCount = 0;
portMUX_TYPE snapshotsMux = portMUX_INITIALIZER_UNLOCKED;

const char* labelName(uint8_t label) {
  switch (static_cast<HeapTrace::Label>(label)) {
    case HeapTrace::Label::PreNetwork: return "pre_network";
    case HeapTrace::Label::AfterApHttpStartup: return "after_ap_http_startup";
    case HeapTrace::Label::BeforeLineBegin: return "before_line_begin";
    case HeapTrace::Label::AfterLineBegin: return "after_line_begin";
    case HeapTrace::Label::LineWorkerBeforeFirstQuota: return "line_before_first_quota_tls";
    case HeapTrace::Label::LineWorkerAfterFirstQuota: return "line_after_first_quota_tls";
    case HeapTrace::Label::LineWorkerBeforeFirstPush: return "line_before_first_push_tls";
    case HeapTrace::Label::LineWorkerAfterFirstPush: return "line_after_first_push_tls";
  }
  return "unknown";
}
}

namespace HeapTrace {
void capture(Label label, uint8_t lineState, bool includeTaskStack) {
  multi_heap_info_t info = {};
  heap_caps_get_info(&info, MALLOC_CAP_8BIT);
  Snapshot sample = {};
  sample.atMs = millis();
  sample.freeBytes = static_cast<uint32_t>(info.total_free_bytes);
  sample.totalBytes = static_cast<uint32_t>(heap_caps_get_total_size(MALLOC_CAP_8BIT));
  sample.minimumFreeBytes = static_cast<uint32_t>(info.minimum_free_bytes);
  sample.largestFreeBlock = static_cast<uint32_t>(info.largest_free_block);
  sample.allocatedBytes = static_cast<uint32_t>(info.total_allocated_bytes);
  sample.freeBlocks = static_cast<uint32_t>(info.free_blocks);
  sample.allocatedBlocks = static_cast<uint32_t>(info.allocated_blocks);
  sample.totalBlocks = static_cast<uint32_t>(info.total_blocks);
  sample.label = static_cast<uint8_t>(label);
  sample.lineState = lineState;
  sample.hasTaskStack = includeTaskStack ? 1 : 0;
  if (includeTaskStack) sample.stackHighWaterBytes = uxTaskGetStackHighWaterMark(nullptr);

  portENTER_CRITICAL(&snapshotsMux);
  if (snapshotCount < kCapacity) snapshots[snapshotCount++] = sample;
  portEXIT_CRITICAL(&snapshotsMux);
}

void print() {
  Snapshot copy[kCapacity] = {};
  uint8_t count = 0;
  portENTER_CRITICAL(&snapshotsMux);
  count = snapshotCount;
  for (uint8_t i = 0; i < count; ++i) copy[i] = snapshots[i];
  portEXIT_CRITICAL(&snapshotsMux);

  Serial.printf("HEAP TRACE: count=%u capacity=%u caps=MALLOC_CAP_8BIT stack_hwm_unit=bytes\n",
                count, kCapacity);
  for (uint8_t i = 0; i < count; ++i) {
    const Snapshot& s = copy[i];
    Serial.printf("HEAP SNAPSHOT: index=%u label=%s ms=%u total=%u free=%u min_free=%u largest=%u allocated=%u free_blocks=%u allocated_blocks=%u total_blocks=%u line_state=%u stack_hwm_bytes=",
                  i, labelName(s.label), s.atMs, s.totalBytes, s.freeBytes,
                  s.minimumFreeBytes, s.largestFreeBlock, s.allocatedBytes,
                  s.freeBlocks, s.allocatedBlocks, s.totalBlocks, s.lineState);
    if (s.hasTaskStack) Serial.printf("%u\n", s.stackHighWaterBytes);
    else Serial.println("na");
  }
  Serial.println("HEAP TRACE END");
}
}
