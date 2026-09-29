#include "SessionManager.h"

#include <bootloader_random.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <string.h>

namespace {
constexpr char kHex[] = "0123456789abcdef";
constexpr uint32_t kMaxTtlMs = 5 * 60 * 1000;
}

size_t SessionManager::index(SessionType type) { return static_cast<size_t>(type); }

bool SessionManager::matches(const char* a, const char* b) {
  if (!a || !b) return false;
  size_t length = strnlen(b, kTokenChars + 1);
  if (length != kTokenChars) return false;
  uint8_t difference = 0;
  for (size_t i = 0; i < kTokenChars; ++i) difference |= static_cast<uint8_t>(a[i] ^ b[i]);
  return difference == 0;
}

bool SessionManager::createSession(SessionType type, uint32_t ttlMs, uint32_t nowMs,
                                   char (&out)[kTokenChars + 1]) {
  const size_t slotIndex = index(type);
  if (slotIndex >= kSessionTypeCount || ttlMs == 0 || ttlMs > kMaxTtlMs) return false;
  invalidateAllOfType(type);
  uint8_t bytes[32];
  // Wi-Fi RF supplies entropy when running. Before Wi-Fi starts, temporarily
  // enable the internal SAR entropy source for this draw.
  wifi_mode_t wifiMode = WIFI_MODE_NULL;
  const bool radioActive = esp_wifi_get_mode(&wifiMode) == ESP_OK &&
                           wifiMode != WIFI_MODE_NULL;
  if (!radioActive) bootloader_random_enable();
  esp_fill_random(bytes, sizeof(bytes));
  if (!radioActive) bootloader_random_disable();
  Slot& slot = slots_[slotIndex];
  for (size_t i = 0; i < sizeof(bytes); ++i) {
    slot.token[i * 2] = kHex[bytes[i] >> 4];
    slot.token[i * 2 + 1] = kHex[bytes[i] & 15];
  }
  slot.token[kTokenChars] = '\0';
  memset(bytes, 0, sizeof(bytes));
  slot.createdMs = nowMs;
  slot.ttlMs = ttlMs;
  slot.valid = true;
  memcpy(out, slot.token, sizeof(slot.token));
  return true;
}

bool SessionManager::active(SessionType type, uint32_t nowMs) const {
  const size_t slotIndex = index(type);
  if (slotIndex >= kSessionTypeCount) return false;
  Slot& slot = slots_[slotIndex];
  if (slot.valid && static_cast<uint32_t>(nowMs - slot.createdMs) >= slot.ttlMs)
    slot.valid = false;
  return slot.valid;
}

bool SessionManager::validateSession(SessionType type, const char* token, uint32_t nowMs) const {
  return active(type, nowMs) && matches(slots_[index(type)].token, token);
}

bool SessionManager::consumeSession(SessionType type, const char* token, uint32_t nowMs) {
  if (!validateSession(type, token, nowMs)) return false;
  invalidateAllOfType(type);
  return true;
}

void SessionManager::invalidateSession(SessionType type, const char* token) {
  const size_t slotIndex = index(type);
  if (slotIndex < kSessionTypeCount && matches(slots_[slotIndex].token, token)) invalidateAllOfType(type);
}

void SessionManager::invalidateAllOfType(SessionType type) {
  const size_t slotIndex = index(type);
  if (slotIndex >= kSessionTypeCount) return;
  memset(&slots_[slotIndex], 0, sizeof(Slot));
}

void SessionManager::expireSessions(uint32_t nowMs) {
  for (size_t i = 0; i < kSessionTypeCount; ++i) {
    if (slots_[i].valid && !active(static_cast<SessionType>(i), nowMs))
      invalidateAllOfType(static_cast<SessionType>(i));
  }
}
