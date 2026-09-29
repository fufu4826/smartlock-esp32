HISTORICAL: Google Sheets is removed from the active product. This document records an earlier design or test checkpoint and is not current implementation authority.

# Google Sheets one-click simplification

Date: 2026-09-27. SOL review checkpoint. Starting commit: `f40efbc3b31c7fd1338378f25a270b9a644fea78`.

## OVERALL

**FAIL - release acceptance remains pending.** Local implementation/cleanup and focused tests pass. Publisher MFA is now cleared. A dedicated Google project, OAuth web client, and Apps Script version 2 deployment have been created; receiver Script Properties are configured. Google application audience remains Testing with the publisher as its sole test user. Anonymous malformed status POST is rejected by the deployed receiver. Real staging consent, automatic Sheet creation, exact upload ACK, duplicate retry and conflicting duplicate rejection passed with synthetic data. The staging callback message and localized default-tab defects were corrected, passed 23 receiver host tests, and deployed as version 3. A second live consent has not retested those corrections. Physical Management G8 and production audience readiness remain pending. No Google-connected board state or hardware deployment approval is claimed. See `evidence/google_one_click/publisher-setup-20260927.md`.

## USER REQUIRED ACTIONS

1. Open SmartLock Management.
2. Tap Connect Google Sheets.
3. Sign in to Google and grant permission.

## SOL architecture decision

**YES, architecturally:** one publisher-preconfigured Apps Script web app, executing as publisher, uses server-side authorization-code OAuth (`openid email drive.file`) and the consenting owner's Google REST token. Google Sheets is the sole remote event history destination. Publisher configuration must exist once before owners can use the flow. This answer does not substitute for successful deployed OAuth or prove that the current account can provision it.

Three tabs: ประวัติการเข้าใช้งาน, เหตุการณ์ระบบ, สถานะระบบ. The receiver creates the spreadsheet, tabs and headers automatically. The device receives safe account display, Sheet link and connection generation through authenticated polling. No Sheet ID, endpoint, token or developer settings are requested from the owner.

## Luna implementation tasks and SOL review

Luna workers inventoried/removed the obsolete architecture and implemented the Google receiver, firmware transport/pairing adaptation, Thai Management flow and tests. SOL reviewed the cleanup identity/evidence and security-sensitive source, identified and corrected pairing-response recovery, byte-based property bounds, full-batch writes, corrupted readback handling, OAuth exchange recovery, reconnect with a pending upload, malformed browser JavaScript and transport stack allocation. Workers hit their usage limits during review; SOL completed the focused corrections and reran tests. This is provisional local review, **not final deployment approval**.

Changed production scope: `src/cloud/CloudProtocol.*`, `CloudPairProtocol.*`, `CloudSync.*`, `CloudTransport.*`, replacement `GoogleReceiverConfig.h`, Google portions of `src/web/WebAssets.h`, and removal of obsolete claim/confirmation routes from `src/network/WebServerManager.cpp`. Existing Google root certificates were reused. Local identity, lock, Admin, touch, Wi-Fi, EventLog and partition implementations were not modified. New receiver source/manifest/tests, build fixture, design, cleanup evidence and historical documentation banners complete the local changes.

## Old architecture cleanup

| Required status | Result |
|---|---|
| OLD CLOUDFLARE WORKER | **NEVER CREATED** in the identified F2 account; official scripts API returned an empty list |
| OLD D1 DATABASE | **NEVER CREATED** in that account; official D1 API returned an empty list |
| OLD POSTGRESQL CLOUD | **NEVER CREATED** by this project; the identified local staging fixture was removed |
| OLD CLOUD DATA | **NEVER CREATED** remotely; project-local PostgreSQL staging container and sole volume removed |
| OLD CLOUD SECRETS | **NEVER CREATED** as Worker secrets; local Wrangler credential removed by logout and project-local staging secret files removed |
| ACTIVE REMOTE HISTORY STORAGE | **GOOGLE SHEETS ONLY** in the active implementation; receiver deployed; synthetic staging history uploaded and verified |

All 75 tracked legacy files under `backend/`, `backend-worker/`, and `cloud/apps_script/` were removed, alongside seven untracked F2 files, deployment configuration, migrations, result logs and generated executable dist files. Five older designs/reports are explicitly historical; Git history is preserved. Generated vendor caches were reversibly archived outside the active repository at `C:\ESP\_archive\smartlock-obsolete-generated-20260927`. Empty dist directories have no runtime content.

The exact Docker container `smartlock-google-stage` and its sole anonymous volume were identified and removed. Unrelated Windows PostgreSQL on port 5432 and other resources were untouched. No remote resource deletion was performed because no project Worker/D1 resource existed. Wrangler submitted token revocation and cleared its local credential; remote grant revocation was not separately inspected. No project backend process or old backend listener was found on the computer at the recorded check. Existing ESP32 firmware was not queried or changed.

Automatic review rejected recursive filesystem deletion and later empty-directory deletion. Cleanup used supported PlatformIO clean, individually reviewed files, package-manager pruning and verified reversible archival. It did not bypass the rejected commands. See `evidence/google_one_click/cleanup.json` and `sol-external-checks.json`.

## Dependencies and zero cost

Cloudflare: **NONE**. D1: **NONE**. PostgreSQL production: **NONE**. workers.dev: **NONE**. External cloud ledger: **NONE**. Paid backend/domain/certificate/billing/card/trial: **NONE required or enabled**. Apps Script/Sheets consumer quotas pause synchronization when exhausted; no paid fallback. The existing optional Cloudflare public NTP hostname is unrelated to cloud history and has no account/deployment requirement.

## Authentication, dedup and SD behavior

Installation-specific 256-bit device secrets travel only in HTTPS POST bodies. Google tokens/client secret stay in publisher Script Properties, never on ESP32 or in Sheet cells. Google host/hostname certificates are verified; a single bounded ContentService redirect receives a bodyless GET without forwarding the device proof. One 20-second processing budget covers DNS/TCP/TLS/body work on the low-priority worker. Main-loop local operations remain independent.

CRC-validated EventLog CSV and stable event IDs are reused. Fixed target-row reservations persist before RAW batch writes. Full row readback precedes the exact seven-field COMMITTED/ALREADY_STORED receipt. Identical events deduplicate; conflicting events reject. Claimed/verified identity and historical snapshots are preserved; formula-like text is written literally. Tokens, PINs, passwords, verifier material and credentials are excluded.

SD retirement still requires the complete matching installation/generation/segment/digest/ordered-event-ID receipt and rereading the unchanged local segment. HTTP 200 alone does not retire it. Lost/malformed responses retain data and back off. Existing protected bounded history and loss counters remain. Storage cannot retain infinite offline history; exhaustion remains visible and does not grant local access. Disconnect does not delete the spreadsheet. Factory Reset policy is covered only by host fixtures; no real reset occurred.

## Test evidence

| Focused suite | Result |
|---|---|
| Receiver core/GAS adapter | **19 tests PASS**, isolated Google/property fakes |
| CloudProtocol | **63 assertions PASS**, production C++ |
| CloudTransport | **55 assertions PASS**, production method with TLS/network fakes |
| CloudSync/pair parser | **44 assertions PASS**, production methods with NVS/SD/queue fakes |
| Reset fixture | **27 assertions PASS**, production reset with isolated storage |
| Google routes | **43 checks PASS**, production handler bodies with dependencies faked |
| Google browser flow | **48 checks PASS**, production JS/browser VM |
| Identity/auth/enrollment | **349 assertions PASS**, 20 migration fault boundaries |
| Security policy/lock ownership | **43 assertions PASS**, source ownership also PASS |
| Admin/gestures/relock | **78 checks + 10 cases PASS**, production methods with physical dependencies faked |
| Network recovery | **101 assertions / 8 scenarios PASS** |
| HTTP deadline | **11 checks PASS** |
| EventLog/Audit/installation fixtures | **PASS**, runner has no aggregate count |
| Registration browser and Admin browser | **PASS**, production JS VM; no physical claims |

Cloud suites total **280 numbered assertions/checks plus 19 receiver tests**; other suites use different counting units and are not inflated into a fabricated total. Receiver coverage includes full 32-row mixed-tab/property-byte bounds, lost write/readback, repeated duplicate/conflict, Thai/literal formula/quotes, wrong/missing proof, malformed/oversized/CRC bodies, expiry cleanup, receiver restart, revoked grant/timeout, reconnect with a partial upload, edited/deleted history and safe callback errors. `receiver-host.txt` records the receiver results. Integrated local regressions passed; they do not prove a real Google integration.

Embedded roots matched Google's published roots and validated the two Google host certificates using Python/OpenSSL on the development computer. **Not ESP32 TLS acceptance.** Real quota behavior, actual consent/redirect/deployment identity, account-change/revocation end-to-end, and realistic receiver-to-firmware wire fixtures remain acceptance work. No test magnet motion or physical PASS was fabricated.

## Resource evidence

Google-enabled build fixture (synthetic URL, **never deploy**): firmware **1,250,521 bytes**, application slot **1,310,720**, headroom **60,199 bytes**, static RAM **118,276 bytes**. Final default build also passed: **1,109,045 bytes**, static RAM **112,016 bytes**; its empty endpoint allows compiler elimination of unavailable cloud transport, so it is not the connected-product budget. No new framework/library or partition change. Compiler stack report: transport post 128 bytes, exchange 368 bytes, worker 1,088 bytes; TLS internals and runtime stack high-water require board measurement. Heap/minimum heap: **NOT MEASURED**, COM6 untouched. Default endpoint is empty and synchronization fails closed until reviewed publisher provisioning.

## Remaining blockers and exact next phase

1. Publisher setup and initial synthetic staging acceptance pass. Callback/localized-tab corrections are host-verified and deployed as version 3; their fresh live consent/creation retest remains pending.
2. Complete production OAuth/branding readiness and perform G8 from an unconnected account through Management → Google consent only. Verify actual Sheet/tabs/headers, pairing receipt and first event. Public OAuth release/branding and grant lifetime must be established.
3. Complete realistic wire/quota/error integration and recovery acceptance. Uncertain spreadsheet creation deliberately pauses if the tagged file cannot be found; it must not blindly create duplicates. The 50,002-row limit pauses rather than providing automatic archives. Manual edits to managed history may stop synchronization. These limitations are documented, not hidden as PASS.
4. Only after these gates and complete SOL approval: separately approved no-reset COM6 deployment, normal boot/persistence, heap/stack and non-destructive cloud checks. Hardware approval is **NOT granted in this run**.

No commit was created: retain the reviewed local checkpoint and explicit release blockers, with unrelated pre-existing edits in FINAL_FROM_ZERO_REPORT, STALE_ENROLLMENT_REPORT and after-phone2.txt preserved. Current HEAD remains the starting commit. The work is not ready to be declared a complete Google product.
