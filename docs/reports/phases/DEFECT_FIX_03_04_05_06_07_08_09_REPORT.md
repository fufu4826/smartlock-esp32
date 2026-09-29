# SmartLock selected defect fix pass

Scope: SL-03 through SL-09 only. Lead security/architecture review with bounded Luna implementation work. SL-01 and SL-02 were not changed. No Factory Reset, Owner registration, PIN/Wi-Fi change, real browser-storage change, Google activation, Bluetooth, OTA/Backup restoration, or partition change occurred.

## Baseline and preservation

Base: `master` at `62ae5c2463fedeb424a3e9aa3b083824cd70818a`. The three pre-existing report/evidence changes listed in `evidence/defect_fix_03_09/repository-baseline.json` are excluded from this commit.

Read-only COM6 baseline: **ko / D000001 / OWNER / ACTIVE**, **hi / D000002 / USER / ACTIVE**; one Owner, two identities, two verifiers. This actual live state supersedes the earlier reported revocation; this run did not revoke hi. Configured YES, PIN store healthy, SD healthy, calibration loaded, GPIO22/LockController LOCKED, five-second unlock duration, STA connected at 192.168.1.179, canonical hostname `smartlock-04225a0ff0a4.local`. Initial boot heap 138,300/minimum 133,004 bytes; later baseline diagnostics 134,124/minimum 110,924. Immediate pre-flash diagnostics confirmed unchanged identities and healthy logs.

Private temporary NVS snapshots are used only for an exact logical-value comparison of configuration, PIN store, AP/STA settings and touch calibration. No secret values or verifier material appear in this report/evidence.

## Findings and lead review

### SL-03 — FIXED: lost registration response

Root cause: Setup/Enrollment can commit and consume the invitation before the browser receives a response. The old browser then depends on retrying a consumed session.

Minimum fix: `RegistrationRecovery` and POST `/api/registration/reconcile` prove the exact pending credential against a unique persisted verifier and an ACTIVE identity. Setup can recover only D000001/Owner; Enrollment excludes Owner and requires LAN ingress. The response contains identity/role metadata only. This path writes no authorization state and never consumes/reuses a QR. Revoked, orphaned, wrong, ambiguous or unhealthy-state proof fails closed.

Browser pending proof is saved separately before submission and promoted only after a valid success/reconciliation. Setup retries preserve the same credential across a fresh QR; pending AP reconnect information is clearly marked unconfirmed. Enrollment only replaces a mismatched pending attempt after definitive failed proof plus a validated fresh invitation; the older attempt is retained separately. Existing active Owner storage survives failures. Legacy pending Setup storage recovers on the first visit.

Evidence: production recovery service and staged production AuthStore lookup tests; production browser scripts with Setup commit/response loss, Enrollment commit/response loss, same-ID recovery, consumed invitation, revoked/wrong proof, fresh-QR retry, transport failure and storage precedence scenarios. Hashing/SD dependencies use explicit host fakes; the deployed build links actual mbedTLS/SD. No real Owner registration was performed.

Lead review: **PASS**. Credential proof recovers existing authority only; there is no new invitation or privilege grant.

### SL-04 — FIXED: Management token revival

Root cause: a retained bearer and modular elapsed-time comparison could appear young after a full uptime wrap.

Minimum fix: a dedicated `ManagementAuth` SessionManager slot permanently retires observed expiry. The loop sweeps it even while physical ADMIN is active. Login replaces the old slot, inactive issuers retire authorization, and reboot has no persisted slot. An incorrect supplied token does not invalidate someone else's valid token. All Management/PIN/network/log mutation gates still use this authority.

Evidence: **33 deterministic production-method assertions**, including just-before/exact/after expiry, crossing wrap, the original apparent-valid window one complete wrap later, replacement login, issuer revocation/reactivation, and fresh-object reboot rejection. Protected-route gate ordering is checked at source level; live unauthorized-route checks are recorded separately.

Lead review: **PASS**, including Management route interactions.

### SL-05 — FIXED: total HTTP deadline

Root cause: per-chunk wait renewal allowed a slow client to monopolize parsing for minutes.

Minimum fix: one **5,000 ms total deadline** starts before the request line and is shared by headers/body/raw reads. Existing 512-byte normal and 1,024-byte Setup caps, header limits, argument limits and MIME checks remain. An empty body avoids allocation; incomplete bodies are rejected and control returns to the loop.

Evidence: **11 deterministic production-helper checks** for fast 512/1,024-byte bodies, trickle, spent-header budget, truncation, zero body and rollover. Live gentle slow-body and fast-body checks are recorded below. Host tests exercise the parser's production helper, not the complete WiFiClient stack.

Lead review: **PASS**, including independent lock timer and fresh UI time use.

### SL-06 — FIXED: Owner credential precedence

Root cause: an older device-array entry could precede the newly committed Owner credential.

Minimum fix: confirmed Setup/reconciliation makes the new Owner authoritative in both browser stores; Access checks active Owner storage first and Management tries it first. Pre-success pending state cannot replace active credentials.

Evidence: production Setup/Access/Management JavaScript VM scenarios cover stale device credentials, successful Setup and lost-response reconciliation. Failed proof preserves active browser data. No actual browser profile was modified.

Lead review: **PASS**, reviewed together with SL-03.

### SL-07 — FIXED: displayed session lifetime

Root cause: TFT session creation reused an old loop timestamp, and browser countdowns subtracted one per callback regardless of actual elapsed time.

Minimum fix: TFT creation reads `millis()` at creation. Browser deadlines conservatively start at request initiation, use current elapsed time, recompute on focus/visibility/callback, hide expired QR links and prevent expired enrollment submission. Nothing extends server authorization.

Evidence: **12 browser/source checks** for delayed responses/callbacks, suspended resume and exact expiry; additionally the actual extracted production `renderCurrentState()` with production SessionManager verifies a fresh 30-second Access session after a simulated 20-second HTTP delay. Existing replay tests pass.

Lead review: **PASS**, including SL-05 interaction.

### SL-08 — FIXED: bounded log retention and traversal

Root cause: the 64 × 32-record outbox stopped accepting new history, and export repeatedly selected the oldest segment.

Minimum fix: deterministic oldest eligible validated closed-segment eviction, at most 64 normal segments (maximum 512 KiB), durable saturating history-loss counters in one atomic eight-byte NVS blob, canonical unique segment IDs, sorted enumeration, named reads and Management selection/export with oldest/newest/loss visibility. Stable event IDs and legacy rows remain readable. Ordinary exports never pin history; the dormant cloud worker pins only an outstanding ACK. New state is included in the existing reset cleanup path without running reset.

Evidence: production EventLog fixtures cover empty/one/multiple segments, the capacity boundary, rotation, ordinary export versus ACK pinning, oldest/middle/newest reads, traversal rejection, durable reboot counters, legacy CRC rows and corruption. Browser export checks cover all three positions and history-loss display.

Limit: loss is intentionally conservative—power loss or delete failure after persisting a planned eviction can overcount. NVS/SD failure is reported as a storage fault; corrupted files are never silently presented as valid history. The product keeps bounded recent history, not infinite retention.

Lead review: **PASS**. Dormant cloud changes only maintain compatibility with new rows; no Google work was activated.

### SL-09 — FIXED: audit attribution and emergency relock

Root cause: denials lost useful identity/source context, and Emergency Unlock did not arm the relock audit sequence.

Minimum fix: `claimed_device_id` is separate from credential-verified `device_id`; only strict safe IDs enter those fields. LAN/Recovery AP attribution uses the request socket's local interface. Physical ADMIN has its own source. Successful emergency unlock arms `EMERGENCY_RELOCK`; normal Access still emits Access/Unlock/Relock. CRC covers the added fields, and no secret is logged.

Evidence: **7 production Access scenarios**, production Audit sequence fixtures (Owner, revoked/invalid/unknown claim, Recovery AP, malformed secret-like claim, emergency relock), and legacy/new CSV parser checks. Hardware magnet movement was not inferred from these fakes or Serial.

Lead review: **PASS**, including SL-08 format/retention interaction.

## Integrated verification and resources

All **16 focused runner invocations passed**; exact commands, outputs and exit codes are in `evidence/defect_fix_03_09/host-regression.txt` and `.json`. Coverage includes production-method identity/enrollment/Access/Management, replay and revoke, ADMIN/PIN/touch/lock, network and configuration, parser helper, EventLog/Audit, browser VM scripts, and gate/GPIO ownership checks. The stale monolithic historical phone2 C++ suite was not used as acceptance evidence.

Console evidence has normalized line endings/trailing whitespace; recorded values and CSV row content are unchanged.

Counted groups: recovery/AuthStore 20; Management 33; deadline 11; EventLog/Audit 100 source assertions; existing cloud protocol compatibility 45; identity 349 plus 20 migration fault boundaries; ADMIN 78 plus 10 router scenarios; network 101; RequestPolicy 43; Access attribution 7 scenarios. Browser deadline has 12 checks and log traversal 14; registration browser scenarios and ADMIN route source checks also pass. Counts distinguish assertions from scenarios rather than implying physical tests.

One release build succeeded. Binary **1,257,216 bytes**, **53,504 bytes** conservative headroom in the unchanged **1,310,720-byte** slot; **17,008 bytes growth**. PlatformIO reports 1,250,641 program bytes and **108,556 static RAM bytes** (+88). No new library or partition change. Pre-flash lead review is recorded in `evidence/defect_fix_03_09/PRE_FLASH_REVIEW.md`.

## Deployment and live acceptance

**PASS: software and non-destructive live-board acceptance.** The approved application image was written to app0 at 0x10000 on COM6 and esptool verified its hash. Bootloader, partition table and authorization/configuration partitions were not flashed. Normal EN/RTS reboots only.

Post-flash boot and read-only diagnostics confirmed GPIO22/LockController LOCKED, configured YES, one Owner, the same two ACTIVE identities and two verifiers, PIN store healthy, five-second unlock duration, SD/calibration healthy, STA reconnected, canonical mDNS initialized, EventLog ready. Access, Management QR, Management bearer, Enrollment and physical ADMIN authorization were all inactive after reboot. No panic/watchdog was observed.

Exact logical NVS comparisons both before and after live HTTP checks passed for **sl-config, sl-pin, sl-net, sl-sta and lock-touch-v2**. This includes the chosen PIN verifier and failure-state bytes, not merely a healthy-store flag. Private snapshots were deleted after comparison. SD authorization checks establish preserved identity/status/count/integrity; they do not export verifier bytes or substitute for actual Owner-phone credential use.

**23 live HTTP checks passed**: health/configured status, retired endpoint rejection, unauthorized Management/enrollment/revoke/PIN/network/log routes, invalid Enrollment/reconciliation/Access proof, fast 512-byte parser body, Setup-sized 900-byte parser body (configured device correctly returns 409), and a single controlled 4-byte/second slow body. The slow request closed at **5.008 seconds**, after 19 body bytes; subsequent `/health` returned 200 in **0.088 seconds**. No authorized grant or credential mutation was sent.

Read-only diagnostics traversed oldest/middle/newest retained segments. **14 live CSV rows across 3 segments passed CRC/event-ID validation**; the deliberately invalid Access request produced a LAN denial with claimed D000001 and an empty verified identity. Legacy and new rows were both readable. Post-HTTP logs were enabled with no storage fault, queue loss or retained-history loss. Canonical mDNS advertised 192.168.1.179 and canonical Host HTTP passed. Windows OS `.local` resolution remains the previously deferred resolver issue.

Resource observations: post-flash boot free heap **136,936**, minimum **132,364** bytes; after HTTP and diagnostics free heap **132,600**, minimum **102,784** bytes. Repeated diagnostics returned the same free heap. These are live snapshots, not a multi-day fragmentation/soak certification. Evidence: `post-flash-boot.txt`, `post-http-live.txt`, `live-http.json`, `live-event-validation.json`, `persistence-comparison.json`, `persistence-after-http.json` and `canonical-live.json` under `evidence/defect_fix_03_09/`.

Lead final software acceptance: **PASS**. Physical-only acceptance remains explicitly pending below.

The final normal-reboot capture (`final-live.txt`) again confirmed all authority slots inactive and GPIO22 LOCKED, with **133,584 bytes free heap / 108,436 minimum** after diagnostics. Thirteen retained segments, zero dropped/history-lost events, no log fault. COM6 was released after capture.

## Files changed

The complete per-file inventory is `evidence/defect_fix_03_09/changed-files.txt`. Production changes are limited to registration recovery/AuthStore, EnrollmentManager/SessionManager, the HTTP parser deadline, WebServerManager/WebAssets, AccessController/Audit/EventLog, fresh-time/diagnostic integration in main, and compatibility bookkeeping in existing reset/cloud modules. Other changes are focused host/browser tests, safe read-only evidence tools and this report/evidence. Pre-existing phase-report edits are not included.

## Physical-only acceptance

Not performed while the user is absent: magnet movement/hold and actual Owner-phone Access unlock/relock; physical entry of the chosen PIN and rejection of the old PIN. Firmware/NVS evidence establishes boot-locked software state and exact persisted PIN data, but does not claim these physical interactions passed. No deliberate authorized live unlock was issued.

The next Google Sheets phase must use the explicit segment IDs, stable event IDs, CRC-validated rows and visible loss counters. It must retain a resource-size gate. No Google service was started in this run.

**Next-phase decision:** the selected software defects and automated live-board checks are complete; the software foundation is ready for Google Sheets development. Full physical acceptance is still pending the two checks above and must not be labeled PASS.
