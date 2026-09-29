# LINE Owner provisioning v2 acceptance

Status: v2 deployed; user-authorized physical-only provisioning adjustment in progress. Real LINE receipt remains PENDING.
Baseline: line-notifications-v1 / 064518abc55002a121a03c7b848a2e635a9afb78; prior evidence f1bdc713d3a880a02defb3492d77239156a74b01.

## Private transfer gate

Synthetic browser-to-helper transfer PASS using the actual in-app browser. A process-generated random synthetic value was revealed only after binding the source page, read into the browser REPL with output suppressed, forwarded as a variable into a masked local helper field and submitted. Boolean evidence: privateReadMatched=true; maskedStateContainsSecret=false; helperAccepted=true; secretAbsent=true. No token literal in tool arguments/output, clipboard, screenshot, browser trace, environment, temporary secret file or Git. Both temporary tabs were closed and REPL references cleared; fixture process stopped. The source is tools/line_provisioning/private_transfer_fixture.py.

This browser exercise used a simulated sink, not COM6. Separate production C++ USB gate/frame host tests and the actual Python helper's simulated serial exchange verify framing and acceptance/rejection. No real LINE secret may be obtained until these tests pass. Real hardware provisioning remains physically gated and is recorded separately below; do not infer it from host tests.

## SOL review scope

Raw Management token/recipient inputs and /api/line/save removed. Owner-only status/test/label/disconnect and maintenance arm/cancel remain. Public Add Friend URL has a fixed LINE origin, percent-encoded validated OA Basic ID, and no recipient/secret. Configuration status never returns recipient/token/fragments/digests.

USB operation is bounded and LINE-specific, uses Owner session revalidation, explicit arm, existing physical Admin authorization, already-locked state, installation binding and random one-use nonce. No unlock/reset/auth/Wi-Fi/general NVS operation. Main loop consumes at most 32 serial bytes per iteration. Frame deadline two seconds; maintenance 120 seconds; nonce retired before mutation. No PIN comparison, gestures or lock output code changed. Existing physical Admin inactivity remains enforced.

Config v2 imports the exact v1 layout. Candidate inactive slot is written and verified before switching the selector; failure tests preserve previous active data. Unknown/corrupt data disables LINE only. Label editing preserves credentials and queue generation; re-provision/disconnect invalidate old retries. Disconnect logically removes inactive/legacy secret records; flash wear-leveling is not cryptographic erasure. Existing device NVS encryption is not claimed.

The helper binds only loopback, checks Host/Origin and SameSite/HttpOnly anti-CSRF cookie, bounds requests and serial deadlines, opens COM6 exclusively with DTR/RTS low, never logs values, keeps payloads in RAM and clears mutable buffers. Host/browser/Python immutable-memory remnants remain an acknowledged local-host risk. Private helper fields are not served by SmartLock Management.

## Preserved behavior

Direct ESP32 to LINE only. No Cloudflare, D1, relay, webhook or external database. Existing three notification hooks, 16-event RAM queue, one-hour TTL, retry UUID, quota, TLS and local-first lock operation preserved. No persistent event history restored. Existing SD auth database and inert history untouched. Google remains absent.

## Evidence

See evidence/line_v2. Pre-deployment read-only diagnostics: one Owner ko/D000001 ACTIVE, hi/D000002 ACTIVE, two verifiers, configured, PIN/SD/calibration healthy, Wi-Fi connected, 5000ms unlock duration, GPIO22 HIGH/LOCKED. Inert history 23 files, 18945 bytes, digest a63df454. Private NVS snapshots remain outside Git and are never printed; compare protected namespaces after deployment, then remove snapshots.

## Outstanding acceptance

Build/deployment evidence will be appended after SOL review. No real OA/token created, no recipient provisioned, no real API test accepted, no human receipt confirmed. Final overall PASS requires all of these; software readiness alone is not final LINE acceptance.

## Pre-flash approval

SOL approves the integrated source for application-only deployment. Seven focused regression commands passed (core LINE, USB gate/frame, private helper, Admin, identity/auth SD, network, security/lock ownership). Node production browser VM and 390x844 Chromium Management checks passed. Browser testing found and fixed disconnect async event.currentTarget use after confirmation. No unrelated production changes staged.

Production build PASS: binary 1,233,632 bytes; slot 1,310,720; headroom 77,088; static RAM 113,564. Source/binary forbidden-term scan PASS; no real LINE secret obtained. Source hash and test details are in evidence/line_v2.

## Deployment and preservation

Implementation commit: 3fc128d8832f51ca8d760f301a68e34c860b3dbb. Tag: line-owner-provisioning-v2. The v1 tag remains unchanged. SOL approved and deployed only firmware.bin at app0 0x10000 using esptool; written hash verified. No partition/NVS erase, Factory Reset, SD formatting or eFuse operation. Normal reboot only.

Live checks PASS: GPIO22 HIGH/LOCKED, configured, ko/D000001 OWNER ACTIVE and hi/D000002 USER ACTIVE, two verifiers, PIN store healthy, 5000 ms unlock duration, SD/calibration healthy, STA 192.168.1.179, canonical hostname and mDNS initialized. Ephemeral Access/Management/enrollment/Admin authorization cleared. Free heap 121,676; minimum 105,408; largest 65,524 bytes. No panic/watchdog. Existing Preferences NOT_FOUND reports the not-yet-created LINE namespace, not authorization loss.

Private before/after NVS comparison: sl-config, sl-pin, sl-net, sl-sta, lock-touch-v2 and sl-install records identical. Secret-bearing snapshots were outside the repository, never printed and removed after comparison. SD identity metadata and inert-history fingerprint match pre-flash: 23 files / 18,945 bytes / a63df454, writes DISABLED.

Live HTTP /health and /manage PASS. Raw credential fields absent; /api/line/save returns 404. All six remaining LINE control/status endpoints reject an invalid Management token with 403. Unarmed LINE_HELLO returns FAILURE; no private material emitted. Final diagnostic after NVS read reboot confirms LOCKED, STA connected, no active Admin authorization and unchanged history.

Access authorization/event hooks are covered by production host regressions; no real door unlock was commanded for acceptance. Actual positive USB provisioning requires Owner arm and physical Admin; it is not claimed from negative live tests. Real LINE API acceptance and human receipt remain untested. Next: official LINE login in the in-app browser, then dedicated Free OA setup and gated private provisioning.

## Physical-only adjustment (latest user authority)

Local Owner session: NOT AVAILABLE to current automation. Both exposed profiles (in-app and Opera) had no Owner Management tab; canonical navigation failed DNS. A normal diagnostic reboot confirmed the same STA IP and recovered numeric /health, but canonical browser navigation still failed. Temporary hosts mapping was denied by Windows and did not change the file. This proves no usable authenticated local session; it does NOT prove every stored browser credential is absent. No phone credential was requested/copied and no credential-origin migration occurred. See evidence/line_v2_physical/local-session-check.json.

The user explicitly authorized physical-only LINE maintenance as the safe alternative. The implementation is narrowly limited to sl-line via the existing bounded USB frame. Normal Owner/role/Management authority is unchanged. No network arming or credential-writing route remains.

Real LINE setup: dedicated SmartLock OA @<redacted-line-basic-id>; provider SmartLock 2005577815; Messaging API channel 2011762201. Account owner completed login/SMS and required agreements. Manager confirms Free plan and 300 monthly messages; no paid upgrade, premium ID, billing or add-on enabled. Webhook absent/disabled; chat, greeting and automatic replies disabled. Owner provider-scoped recipient and one long-lived token held only in private browser-runtime memory, never printed or written to source/report. The console reports the creator account was automatically added as friend; human receipt remains required and is not inferred from that count.

## Physical-only integrated approval

SOL reviewed all source changes: PIN-authenticated explicit LINE screen, locked-only gate, fixed 120-second timeout, one-use installation-bound USB nonce, no HTTP arming/credential routes and no changes to LockController or auth storage. Focused Admin (81 checks plus router/gesture), LINE core, USB frame, helper, mobile/VM and preservation regressions passed. Production binary 1,232,992 bytes; headroom 77,728; static RAM 113,500 bytes. Active source/binary forbidden Google and retired LINE route scan passed. Approved application-only COM6 deployment. Positive provisioning and human receipt remain pending.

## Physical-only deployment evidence

Commit 48c653e, tag line-owner-provisioning-v2-physical. Application-only flash at 0x10000 verified. Live boot/HTTP checks PASS: locked GPIO22/LockController, Owner and second identity unchanged, two verifiers, PIN/SD/calibration healthy, configured, Wi-Fi connected and mDNS initialized, 5000ms duration preserved. Private NVS comparison confirms identical sl-config/sl-pin/sl-net/sl-sta/lock-touch-v2/sl-install records; temporary snapshots removed. Inert SD history fingerprint remains unchanged. Old save/arm/cancel routes return 404; remaining Owner LINE routes reject invalid authorization. Private helper staged, no real credentials committed to device yet. Awaiting human PIN-authenticated explicit LINE setup screen, followed by private USB provisioning and actual message receipt.

## Real provisioning and cold TLS defect

Private USB transaction succeeded: configured=true, version=2; no secrets echoed. First cold TLS quota request exposed a line-worker stack canary overflow. Boot remained LOCKED and configuration persisted. SOL reviewed the minimal 12KB-to-24KB isolated worker stack increase; Luna independent review confirmed cumulative HTTP buffers plus mbedTLS handshake stack pressure. LINE host regression and production build passed; commit 14645a9 deployed application-only with verified flash hash. Live real TLS quota/usage requests subsequently reached Ready (state=1), no observed panic over verification window. Free heap 109284, minimum 54944, largest block 55284; binary 1232992, headroom 77728, static RAM 113500. Owner/identities/PIN/Wi-Fi/calibration/installation/SD health and inert-history fingerprint preserved. Original test was lost on reboot by the intentional RAM-only policy; a new gated test and human receipt remain pending. Background helper is ready and opens COM6 only on submission.

## Real LINE push accepted

Fresh human physical Admin confirmation authorized one private USB transaction; SUCCESS configured=1 version=2. Actual hardware test reached test_state=2, sent=1, queue=0, failed=0, state=Ready. This means LINE API accepted the request, not proof of delivery/read. No panic during quota/TLS/push; GPIO22 remained HIGH/LOCKED, core state healthy, inert SD history unchanged. Private helper exited after success; form buffers and browser temporary secret references cleared. Human message receipt remains the only final acceptance gap.

## Final acceptance

OVERALL: PASS. User explicitly confirmed YES that the real test notification arrived in LINE. Direct ESP32-to-LINE only, Free plan, no relay/Cloudflare/D1. Owner did not enter token or recipient ID. Existing OA friend relationship and actual receipt verified. Real API acceptance and human receipt are separately evidenced. Final firmware source commit 14645a9; implementation and preservation evidence are in this report and evidence/line_v2_physical. Raw token/User-ID UI and HTTP save/arming routes removed. Physical Admin-only USB maintenance stays locked-only, single-use, 120 seconds, sl-line only. Core state preserved; no lock commanded by setup. SD event history remains disabled; Google absent. No remaining setup blocker.
