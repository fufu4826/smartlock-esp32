#pragma once
#include "EventLog.h"

namespace Audit {
bool begin(bool sdAvailable);
void record(smartlock::events::Action action, smartlock::events::Result result,
            const char* userId = "", const char* deviceId = "",
            smartlock::events::Source source = smartlock::events::Source::Unknown,
            const char* claimedDeviceId = "");
void denied(const char* claimedDeviceId, const char* verifiedDeviceId,
            smartlock::events::Source source,
            smartlock::events::Result result = smartlock::events::Result::Denied);
void granted(const char* userId, const char* deviceId,
             smartlock::events::Source source = smartlock::events::Source::Unknown);
void emergencyGranted();
void setSystemDiagnostic(void (*callback)());
void update(bool locked, bool staConnected, uint32_t nowMs);
void pollDiagnostics();
smartlock::events::EventLog* log();
}
