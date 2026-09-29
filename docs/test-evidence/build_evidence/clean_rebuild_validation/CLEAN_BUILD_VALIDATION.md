# Offline cleanup validation — 2026-09-28

The post-acceptance source checkout built successfully with `pio run -e esp32_035` while the ESP32 was physically disconnected. PlatformIO reported 1,232,177 bytes of flash used of 1,310,720 (94.0%) and 112,612 bytes of RAM used of 327,680 (34.4%). The generated clean-build firmware is a validation artifact and is not the accepted release image; the accepted image remains SHA-256 `084227e2e77994ae21b773e253c209b8ba49b897354b3969a5f76f328cb06be9`.

Passing host checks: line notification, admin (93 focused checks plus 10 router cases), security policy (43 assertions), canonical origin (20 assertions), network manager (155 assertions), registration and AuthStore, identity (349 assertions plus 20 migration boundaries), HTTP deadline (11 checks), and touch/state regression.

Deferred legacy suites: `tests/events/run_event_log_tests.py` and `tests/phone2/run_phone2_tests.py` fail to compile because `src/events/EventLog.h` and `src/events/EventLog.cpp` are absent from the accepted source tree. These suites point at the same absent files and were not repaired during post-acceptance cleanup. No firmware source was changed to make them pass.

No upload, reset, serial monitor, or COM-port action was performed.
