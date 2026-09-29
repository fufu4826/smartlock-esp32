# Phase 3 — QR engine and temporary sessions

Status: **PASS**.

## Work

- Sol Light implemented `SessionManager`, its integration with configured and unconfigured tap states, and the session/security review.
- Luna High implemented the bounded `QrManager` renderer and pinned QRCode dependency. Sol Light reviewed and tightened payload bounds and encoder error handling.
- Every session has a fresh 256-bit random token kept in RAM, a type, and a monotonic TTL. Session validation rejects wrong, expired, replaced, consumed, and invalidated tokens. No token is printed to Serial.
- With radio off, token generation enables the ESP32 hardware entropy source during each draw. SessionManager releases it immediately afterward.
- Unconfigured single tap renders a Setup QR. Configured single tap renders Access QR. Configured triple tap invalidates Access and renders Management QR. The current board configuration remains unconfigured; configured transition behavior is exercised in the boot self-test.
- The QR uses a version 10 matrix, four-module white quiet zone, black modules, and a short status label on the 320×480 TFT. The Phase 3 URL is a preview of the Phase 4 setup endpoint; no Wi-Fi server exists in this phase.
- GPIO22 lock control remains unchanged and defaults to LOCKED. No QR/session path calls unlock.

## Verification

- `pio run`: PASS.
- `pio run -t upload --upload-port COM6`: PASS on ESP32-D0WD-V3.
- Serial boot: `LockController: LOCKED`, `SD: OK (30000 MB)`, `SESSION TEST: PASS`, `SCREEN: OFF`, `READY`.
- Boot self-test covers unique replacement tokens, invalid old token, wrong type, expiry, consumption/replay, unconfigured Setup transition, configured Access/Management transition, Access invalidation on Management transition, and Management timeout.
- Physical QR optical scan: PASS, user confirmed SETUP label and a phone camera decoded a URL beginning `http://192.168.4.1/setup?session=`.
- Physical magnetic lock check: PASS, user confirmed the magnet remained energized and no unlock occurred.
- Final encoder error-handling change was recompiled and flashed to COM6; Serial again showed `SESSION TEST: PASS` and `LockController: LOCKED`.

## Decision

Sol Light independently reviewed session creation, replay/expiry rejection, transition invalidation, QR bounds and error handling, and GPIO22 usage. Phase 3 **PASSES**. Phase 4 may begin.
