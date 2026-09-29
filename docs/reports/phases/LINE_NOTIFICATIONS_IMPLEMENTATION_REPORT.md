# LINE notifications implementation

OVERALL: PASS ? software and non-destructive deployment acceptance complete. Real LINE delivery awaits Owner configuration.

## Scope and SOL approval
Owner-only Thai Management LINE provisioning replaces the L0 USB-only proposal. Existing local HTTP confidentiality tradeoff is explicitly accepted by the Owner. No LINE account, real token, recipient, paid plan or cloud service was created. Outbound notifications cannot control the lock.

SOL reviewed all production changes: Access authorization unchanged; enqueue follows successful new LockController unlock only. Wrong-PIN callback follows completed failed comparison and preserves durable security state. Emergency hook follows successful physical unlock only. Only LockController writes GPIO22. Management POST endpoints require existing valid Management authority and current Owner role. Token never returned, rendered after saving, logged or stored in browser storage. Fixed HTTPS destination, validated DigiCert Global Root G3, hostname/SNI validation, bounded worker requests, stable per-event retry UUID. Separate sl-line configuration blob; no persistent event payloads.

## Delivery
16 total RAM events including in-flight, FIFO, oldest waiting dropped on overflow, one-hour TTL, bounded attempts/backoff. Reboot loses pending notifications by accepted policy. Configuration changes invalidate old retries. 200 requires complete valid JSON; 409 requires accepted-request-id. Authentication errors pause; quota APIs require limited Free-plan quota <=300 and pause at exhaustion. No delivery/read guarantee. Local HTTP token provisioning and unencrypted device NVS remain physical/LAN confidentiality limitations.

## Event history removal
Active Audit/EventLog source moved into tests/archive/event_history as historical material. History routes/UI removed. Existing SD history is untouched/inert. Identity/auth SD persistence and backup/integrity operations remain. TimeManager and read-only USB diagnostics remain. No LINE or event queue writes to SD/NVS.

## Focused verification
All eight integrated commands passed: production LINE method/parser/config/queue tests; Admin tests including failed comparison callback; production Management JavaScript tests; production identity/Access/Management cleanup smoke; full identity persistence/integrity tests; Wi-Fi/network tests; security/lock ownership checks; HTTP total-deadline tests. See evidence/line_v1/regression.json and regression.txt. Mobile Chromium 390x844 used production Management HTML and synthetic Owner/Admin API fixtures: secret cleared, no readback/storage, queued vs accepted distinguished, disconnect POST and Admin exclusion passed. No physical unlock claimed.

## Resources
Actual firmware.bin: 1,224,528 bytes. Application slot: 1,310,720. Headroom: 86,192 bytes. PlatformIO linked flash usage: 1,217,957. Static RAM: 112,348 bytes. Final build PASS. Production search Google/googleapis/script.google/google-v1/Audit/EventLog/setInsecure returned zero matches. Public CA verified against official DigiCert download.

## Deployment plan
Existing live partition table equals build table; active app0 at 0x10000. Application-only write preserves bootloader, partition table, NVS, SD. Read-only baseline: exactly one Owner ko/D000001; hi/D000002 ACTIVE; two verifiers; configured; PIN store healthy; unlock 5000ms; Wi-Fi, calibration and SD healthy; GPIO22 HIGH/locked. Private NVS snapshot stays outside Git and will be compared without exposing secrets. No reset, erase, credentials, PIN or Wi-Fi changes.

## Remaining acceptance
Real LINE API acceptance requires future Owner OA/token/recipient configuration. Physical magnet movement and actual registered Owner phone interaction are not claimed by host tests or Serial. Final live evidence will be appended after deployment.

## Final COM6 acceptance
Source checkpoint: 064518a; tag line-notifications-v1. Application-only flash at 0x10000 completed; esptool verified written hash. Partition table prefix matches build exactly with erased padding. No erase/reset/format/partition change. Normal reboot only.

Live boot: LockController LOCKED; GPIO22 HIGH/LOCKED; configured YES; one Owner ko/D000001 ACTIVE, hi/D000002 ACTIVE (preserved actual baseline, not assumed revoked); identities 2, verifiers 2. SD and PIN stores healthy; unlock duration 5000ms; calibration loaded; canonical hostname/mDNS initialized; HTTP ready; STA 192.168.1.179. Temporary Access/Management/Enrollment/Admin authority absent. No panic/watchdog observed.

Protected NVS comparison: sl-config, sl-pin, sl-net, sl-sta, lock-touch-v2, sl-install entries identical before/after; private snapshots never committed and deleted after comparison. Identity/verifier metadata remains consistent. No credentials read into chat or reports.

Live health 200; /manage 200 includes Thai LINE page, excludes Google/history API UI. Invalid Management bearer /api/line/status 403; retired /api/logs/status 404. LINE disabled/unconfigured, queue/sent/failed all zero. Existing inert history stayed 23 files / 18,945 bytes / digest a63df454 across multiple diagnostics and reboot. No new operational SD history writes. Final free heap 123,268 bytes, minimum 117,324, largest block 69,620; initial deployment-observation minimum 105,484.

Real Owner Access authorization/unlock/relock production-method regressions passed; no real authorized unlock or physical magnet movement was requested/performed. Actual registered phone interaction and real LINE token/API delivery remain future configuration acceptance, not fabricated PASS. No LINE account creation, Google request, or paid service action.

NEXT USER ACTION: Open existing Owner Management ? ???? ? ???????????? LINE. Do not send tokens/user IDs in chat; configure through Owner-only device form.
