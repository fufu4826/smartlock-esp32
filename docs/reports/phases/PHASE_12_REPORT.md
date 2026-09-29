HISTORICAL: Google Sheets is removed from the active product. This document records an earlier design or test checkpoint and is not current implementation authority.

# Phase 12 — optional Google Sheets event synchronization

Status: implementation and isolated tests prepared; real Google deployment/integration checkpoint pending. Do not mark end-to-end cloud acceptance PASS.

User priority update (2026-09-26): manual Google deployment is PAUSED. The required next checkpoint is LAN-primary networking and one real Phone #2 enrollment/access/revoke flow; see `LAN_PHONE2_CHECKPOINT.md`. Earlier instructions below to deploy Google before proceeding are superseded by this user request. Preserve the cloud implementation without building a production OAuth/backend now.

## Design and ownership

Sol implemented and reviewed cloud configuration storage, Owner authorization, TLS trust/redirect policy, queue ownership, ACK validation and deletion semantics. Luna supplied Apps Script boilerplate, Thai UI, and bounded host tests; Sol required receiver schema/deduplication corrections and independently ran the tests.

Cloud starts disabled and unconfigured. Owner-only Management configuration accepts an exact HTTPS Apps Script `/macros/s/<deployment>/exec` URL and 32–128 hexadecimal-character ingestion secret. Alternating CRC/readback NVS slots preserve valid configuration; API status never returns the secret. Factory reset includes the cloud namespace but was not invoked. The ingestion secret is separate from Owner/device credentials, never an event field, and is not stored in browser localStorage.

The loop selects a CRC-validated closed SD segment, serializes only the approved event columns, then sends an immutable in-memory job to a separate FreeRTOS HTTPS worker. The worker has no SD, session, authorization, display or lock operation. Time synchronization is required for TLS; local authorization continues independently when time/network/Google is unavailable.

TLS uses Google Trust Services R1/R4 roots with hostname verification; no insecure mode is used. The POST target is restricted to script.google.com. A 302/303 ContentService response can be followed only by a GET to exact script.googleusercontent.com; the ingestion body is not forwarded. Responses are bounded to 4096 bytes. ACK parsing requires `ok:true`, the exact batch ID and every event ID in order, with no extras/trailing content. Unrecognized JSON ordering conservatively retries rather than deletes.

Only the main loop can delete a segment, after matching ACK, reselecting/revalidating that same segment, reconstructing the same payload and checking its complete content and event IDs. Export cannot acknowledge/delete data. No cloud path names or deletes Users/Devices/auth files. Failed/partial/lost ACKs retain the local segment and use bounded exponential retry, capped at 16 minutes. Stable event IDs make retries idempotent.

Receiver uses a ScriptLock, validates the exact event schema and existing Sheet rows, rejects same-ID/different-content collisions and duplicate stored IDs, writes only missing rows, flushes and verifies readback before ACK. A 50,000-row bound makes capacity failure explicit; rollover/archival must be planned before that bound, without deleting unacknowledged local data.

## Tests completed before real deployment

- Native production CloudProtocol tests: 38 assertions across three groups PASS (URL/secret bounds; complete ACK; malformed/partial/duplicate/wrong batch/trailing/oversize rejection).
- Apps Script Node mocks PASS: append, lost-ACK retry deduplication, collision rejection, offline events, malformed payload/history, auth rejection and body limits.
- Existing fake-SD tests cover exact selected/read-validated basename deletion, active segment rejection, CRC/duplicate detection and retained partial files. They never touch the user's real SD database.
- Desktop TLS connections to script.google.com and script.googleusercontent.com validated using precisely the bundled R1/R4 roots and hostname checks. This is desktop trust verification, not ESP32 live deployment proof.
- Thai UI UTF-8, embedded JS syntax and diff checks pass. No Google URL or secret was invented, configured, or deployed.

## Genuine user checkpoint

The remaining end-to-end test requires a real Apps Script deployment, Sheet identifier and ingestion secret. Follow `cloud/apps_script/README.md`; enter the secret only in Script Properties and the existing Owner Management → Cloud form. A deployment URL may be shared, but do not send the secret in chat. Cloud remains disabled until configured intentionally.

Real HTTPS upload, receiver permissions, ACK deletion, lost-ACK retry against Google, configured-mode heap and network-failure runtime tests remain pending. Phase 13/14 execution resumes after this explicit checkpoint, as required by the user's execution policy. Nonblocking physical checks remain consolidated in Phase 14.

## Primary references

- Google ContentService redirect behavior: https://developers.google.com/apps-script/guides/content
- Trust repository: https://pki.goog/repository/
- Bundled roots: https://pki.goog/repo/certs/gtsr1.pem and https://pki.goog/repo/certs/gtsr4.pem (retrieved 2026-09-26).

## Runtime checkpoint — paused for connectivity (2026-09-26)

Phase 12 build/upload COM6 passed in 92.87 seconds, hash verified; RAM 119740/327680 bytes, flash 1232261/1310720 bytes. Serial after monitored reboot reports GPIO22 LOCKED, timer OK, configured/Owner YES, database PASS users=1 devices=1 active=1 revoked=0, saved STA 192.168.1.179, AUDIT READY, cloud disabled/unconfigured/busy=false, free heap 128004 and minimum 122784. No unexpected unlock or boot instability was observed. USB audit still validates persisted events, NTP=1, no fault and no dropped records.

However Windows canonical and numeric-IP HTTP probes both timed out repeatedly, although OS resolution returned the expected LAN IP. Therefore Phase 12 runtime acceptance is NOT PASS. Per the user's explicit stop condition 11 (cannot reliably reconnect after flashing), further flashes and phase progression are paused and the user was asked to check the existing Owner phone on home Wi-Fi. No factory reset, real cloud configuration, replacement credential or database modification was attempted.

Two small source changes after that flashed build (avoiding an empty NVS namespace NOT_FOUND diagnostic and the Thai Phase 12 firmware label correction) remain unflashed. Phase 12 work is preserved uncommitted pending connectivity resolution; no Phase 12 completion or live Google acceptance is claimed. The real Google deployment checkpoint will follow after reliable local access is restored.

## Connectivity checkpoint resolved by user confirmation

The user confirmed both the canonical homepage and numeric-IP /health open from the existing phone on home Wi-Fi. This establishes phone-side LAN reachability; the earlier Windows timeouts are tracked as a Windows-side connectivity/resolution regression, not evidence of lost ESP32 connectivity. No reset, re-registration, Owner replacement, database change or STA reconfiguration is required or performed.

On resumption, Windows numeric-IP Phase 12 preflight also passed: unauthorized cloud/status, cloud/config and logs/status/export requests were denied, malformed cloud configuration was rejected, and the Thai UI shell was delivered. The 38 native protocol assertions, receiver mock suite, JS syntax and UTF-8 checks were rerun successfully. The previous connectivity pause is resolved; the remaining genuine external checkpoint is the real Google deployment.

## Final implementation checkpoint (2026-09-26)

The final source, including the Thai firmware label and cloud namespace initialization, was compiled and flashed to COM6 successfully in 96.14 seconds; flash hash verified. RAM: 119740/327680 bytes. Flash: 1232341/1310720 bytes (94.0%). This supersedes the earlier note about unflashed changes.

Monitored reboot confirms Phase 12 firmware, GPIO22 LOCKED, lock timer OK, configured YES, Owner YES, database PASS with users=1 devices=1 active=1 revoked=0, SD OK, canonical mDNS/HTTP initialized and saved STA connected at 192.168.1.179. Cloud remains disabled/unconfigured/not busy. Free heap was 127952 bytes, minimum 122732. The absent cloud configuration slots a/b produce nonfatal NOT_FOUND diagnostics on this unconfigured device; these are not missing Owner/database records.

The read-only USB audit probe passed after this flash: eight retained segments, no storage fault, no dropped records, NTP synchronized, and the original two BOOT/NETWORK_CHANGED records retained their event IDs and valid CRCs. No factory reset, re-registration, Owner/device mutation, Wi-Fi reconfiguration or unlock was performed.

Windows numeric-IP HTTP preflight timed out again after the final reboot. The earlier same-turn API preflight passed before the final minor source changes; do not represent it as a successful post-final-flash HTTP test. Per the user's phone confirmation and explicit instruction, track this as an unresolved Windows-side regression, not evidence that the ESP32 lost LAN connectivity. USB startup and audit evidence are current; phone endpoint reachability was confirmed by the user before this final flash.

Final Sol review: Owner-only cloud configuration, restricted TLS endpoints, worker isolation from lock/auth/SD, complete ACK matching, revalidation before segment deletion, and retention of authentication data reviewed. Commit this as an implementation checkpoint, not full Phase 12 acceptance. Actual Google deployment, configured-worker heap/runtime, upload/ACK/deletion and failure/retry checks remain required before Phase 12 PASS. The deferred independent Phone #2/#3 enrollment/revoke test remains on Phase 14.
