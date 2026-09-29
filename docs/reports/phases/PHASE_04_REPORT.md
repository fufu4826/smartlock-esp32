# Phase 4 — SoftAP, HTTP and captive portal

Status: **PASS**.

## Work and Sol Light review

- Sol Light defined and implemented the first-setup network boundary: only an unconfigured device starts the open `SmartLock-XXXX` SoftAP at `192.168.4.1`. A configured device does not start this open AP.
- Sol Light implemented wildcard DNS captive-portal handling. It changes no lock or configuration state.
- Luna High implemented GET-only HTTP routes, informational HTML, status and health responses, and common captive detection redirects. Sol Light independently reviewed all routes. `/setup` does not consume or echo its QR session query; no route creates an owner or unlocks.
- The loop services DNS, HTTP, touch and the lock controller without a blocking network wait. GPIO22 lock commands were not changed.

## Automated and board evidence

- Integrated `pio run`: PASS.
- Final `pio run -t upload --upload-port COM6`: PASS.
- Serial: `LockController: LOCKED`, `SD: OK (30000 MB)`, `SESSION TEST: PASS`, `AP: OK SSID SmartLock-F0A4 IP 192.168.4.1`, `DNS: OK`, `HTTP: OK`, `READY`. Touch later logged `SCREEN: SETUP QR`.
- PC saw open `SmartLock-F0A4` at strong signal and received DHCP address `192.168.4.4` on connection.
- Direct HTTP returned 200 for `/`, `/setup`, `/api/status`, `/health`; common Android and Apple captive checks returned 302.
- DNS wildcard resolved a test hostname to `192.168.4.1`.
- `POST /setup` returned 404; a query token was not echoed in the setup page. Status JSON contained no session token.
- Sequential `/health` requests: 50/50 PASS. The PC's original Wi-Fi profile was restored after testing.
- Physical phone: PASS. The user connected to the open AP, Android launched the captive portal, and the SmartLock setup page opened without Internet.
- The user also observed `/api/status` returning `{"configured":false,"state":"setup"}`, confirming the read-only status API. This is intentional: captive detection URLs redirect to the local site; `/api/status` is an explicitly registered separate JSON route and contains no session token.
- TFT/touch and GPIO22: PASS. The user tapped once and saw the Setup QR while the magnet remained energized and locked. No unintended unlock occurred.

## Decision

Sol Light reviewed AP startup conditions, all routes, captive redirects, token handling, and lock call sites. All Phase 4 acceptance criteria pass. Phase 5 may begin.
