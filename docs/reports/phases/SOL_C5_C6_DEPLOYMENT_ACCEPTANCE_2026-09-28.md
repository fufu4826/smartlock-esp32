# Sol C5/C6 deployment and acceptance — 2026-09-28

## Starting state and scope

- Starting branch `master`, HEAD `8c15375781eab2f54e9d4f573c355ec2243a294b`.
- Pre-existing unrelated tracked WIP: `CANONICAL_RECURRENCE_INVESTIGATION.md`, `FINAL_FROM_ZERO_REPORT.md`, and `STALE_ENROLLMENT_REPORT.md`; existing untracked architecture, phase/evidence files and tools were left untouched. The previous Sol report was preserved.
- The only approved correction was the six-file C5/C6 source/test diff in `src/main.cpp`, `src/security/AdminPin.{cpp,h}`, `tests/admin/admin_test.cpp`, `tests/admin/admin_transition_test.cpp`, and `tests/admin/setup_backend_test.py`. No unexplained source diff was present before validation.

## Source review and checkpoint

Sol rechecked that an existing valid PIN verifier is not overwritten, an unhealthy installation does not qualify as fresh setup, creation requires affirmative fresh-install evidence, and ambiguous or corrupt evidence fails closed. No schema migration or secret logging was added. The physical Admin PIN and explicit Emergency Unlock action remain prerequisites; healthy local identity/auth, valid duration, available LockController timer and locked GPIO/software state remain checked. AP, STA, mDNS, HTTP, DNS, TLS and LINE readiness do not authorize or block the local action. GPIO22 HIGH remains locked, LOW unlocked; LockController remains the production writer and arms its timer before driving LOW. No LINE/network route gained unlock authority.

Only those six files were committed as `d1e5ce38058790a973ce90c1addf33ef206ce4cd` (`fix: harden admin pin bootstrap and emergency unlock gating`). No tag was created. Unrelated WIP was not staged or committed.

## Repeatable validation

Luna High completed a clean `pio run -e esp32_035 -t clean` then `pio run -e esp32_035`: PASS. Admin 93 checks and 10 router/gesture cases, web and browser setup harnesses, setup backend, identity 349 assertions with 20 migration fault boundaries, RequestPolicy 43 checks and GPIO ownership check all PASS. `git diff --check` passed with line-ending warnings only.

Build RAM: 112,612 / 327,680 bytes. Linked app flash: 1,232,177 / 1,310,720 bytes. Fresh `firmware.bin`: 1,238,752 bytes, SHA-256 `084227e2e77994ae21b773e253c209b8ba49b897354b3969a5f76f328cb06be9`.

## Port and deployment gate

Windows PnP and PlatformIO each list one present serial adapter: COM6, USB-SERIAL CH340, VID:PID `1A86:7523`, PnP ID `USB\\VID_1A86&PID_7523\\6&3AFC7336&0&2`, location 1-2. A read-only esptool chip probe reported ESP32-D0WD-V3 rev 3.1 and 40 MHz crystal. Luna compared its device MAC privately against the prior accepted `evidence/admin_menu_fix/flash-verify.txt` and found an exact match; the MAC is omitted here. Sol accepts the unique historical board identity. A live partition read at `0x8000` (3,072 bytes) hashes to `148b959cbff1c38aa8e1d5c0ba9d612c54997b945e56a63f41223eef650653a1`, matching both the saved accepted image and current build. A live otadata read at `0xe000` (8,192 bytes) hashes to `f94c5d786a7a8fab06ac5d10e33bf37711a6697636dc037559ea19cc410a17f0`, matching the saved accepted app0 selection (OTA sequence 1, app0 at `0x10000`). Sol approved application-only deployment after these read-only gates.

The established safe method was application-only `esptool.py write_flash` to app0 at `0x10000`, with no bootloader, partition table, otadata, NVS or SD image argument. The exact command and sanitized result are in `evidence/c5_c6_deployment/command-results.txt`. Esptool v4.11.0 erased only `0x10000–0x13efff`, wrote 1,238,752 bytes and reported `Hash of data verified`. A separate read-only `verify_flash 0x10000 firmware.bin` returned `verify OK (digest matched)` over all 1,238,752 bytes. A normal RTS reset then booted the application. No full-device erase, Factory Reset, credential change, enrollment, calibration or physical event occurred.

## Live acceptance

Flash and readback: PASS, source commit and artifact hash above. Running-image provenance: verified app0 image at `0x10000` plus post-reset boot of the SmartLock application; a runtime self-reported commit ID was not available. Boot locked software state: PASS (`LOCKED`, GPIO22 locked, timer ready); physical magnetic holding force remains untested. Pre/post redacted `DIAG_AUTH` and `DIAG_SYSTEM` values match: valid auth, two identities, one Owner, two verifiers, configured and Owner present, 5,000-ms unlock, SD/calibration/PIN store healthy, locked GPIO22, STA connected, Admin inactive, and no panic/watchdog markers. These prove non-secret state/health continuity, not byte-for-byte NVS/SD preservation. Raw NVS/SD digests were not collected; the application-only write range excludes those areas.

A further fixed read-only `DIAG_SYSTEM` observed installation metadata healthy, mDNS and HTTP ready on the saved canonical host, protected AP enabled with no clients, and LINE configured/enabled with configuration version 2. No secret or installation identifier was recorded. The LINE queue and generated/enqueued event counts were zero, so no delivery receipt was tested. Heap free/min/largest blocks were 109,552/55,268/34,804 bytes; the LINE heap-blocked counter was 214. The largest block is below the existing 40,000-byte admission threshold, a current delivery-risk signal. The counter is not attributable to a newly generated event in this pass, and no causal change or notification receipt can be claimed. The extended network/LINE/heap fields were not retained preflash, so exact pre/post comparison is unavailable. Inert SD history postflash was 23 files, 18,945 bytes, digest `a63df454`, writes disabled; prior saved evidence matches these values, but that evidence is historical. Current physical magnet operation is UNKNOWN. No destructive operation occurred in this work package.

Evidence: `evidence/c5_c6_deployment/command-results.txt`, `deployment.json`, `state-comparison.json`, `postflash-extended-diagnostic.json`, and read-only partition/otadata captures. Sol inspected the actual sanitized command and comparison records, not only Luna's summary.

**Sol software/deployment verdict: PASS for the C5/C6 image, write/readback and observed state continuity. End-to-end acceptance: FAIL/PENDING.** Physical lock behavior, Owner Android canonical path and three real LINE receipts remain untested. The fresh heap admission signal means LINE delivery must not be assumed working; investigate it in a separate approved work package if it persists during the user-driven event phase. This bounded pass ends here.

The remaining user physical acceptance sequence is:

1. On the registered Owner Android/browser, open the normal canonical Access QR path on the intended home LAN. Confirm page/auth, actual magnetic release, timed physical relock and one `UNLOCK_SUCCESS` LINE receipt.
2. Make exactly one deliberately wrong physical Admin PIN attempt. Confirm no physical unlock and an `ADMIN_PIN_FAILED` LINE receipt; avoid unnecessary further wrong attempts.
3. Enter the correct Admin PIN physically, select Emergency Unlock, and confirm actual magnetic release, timed physical relock and an `ADMIN_EMERGENCY_UNLOCK` LINE receipt.
4. Confirm the registered Owner Android can use the canonical SmartLock origin on the home LAN. If protected fallback AP is in the acceptance scope, verify it separately without clearing Owner browser storage.

None of these actions was performed in this passive deployment pass. Because current LINE admission capacity is below its configured threshold, a missing receipt must be recorded as FAIL and must not be inferred from HTTP/API success.
