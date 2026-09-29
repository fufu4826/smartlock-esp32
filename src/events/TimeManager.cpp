#include "TimeManager.h"
#include <Arduino.h>
#include <time.h>
#include <esp_sntp.h>

namespace {
volatile bool receivedNtp = false;
volatile uint32_t synchronizedAt = 0;
bool started = false;
void synchronizedClock(struct timeval*) {
  synchronizedAt = millis();
  receivedNtp = true;
}
}

void TimeManager::update(bool staConnected, uint32_t) {
  if (!staConnected || started) return;
  sntp_set_time_sync_notification_cb(synchronizedClock);
  // SNTP runs outside the loop; no blocking Internet request on the lock path.
  configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
  started = true;
}

bool TimeManager::synchronized() {
  const time_t current = time(nullptr);
  return receivedNtp && static_cast<uint32_t>(millis() - synchronizedAt) < 86400000UL &&
         current >= 1704067200LL && current < 4102444800LL;
}

uint32_t TimeManager::epochSeconds() {
  return synchronized() ? static_cast<uint32_t>(time(nullptr)) : 0;
}
