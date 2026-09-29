HISTORICAL: Google Sheets is removed from the active product. This document records an earlier design or test checkpoint and is not current implementation authority.

# Google Sheets complete implementation report

> **HISTORICAL / SUPERSEDED (2026-09-27):** This report records the earlier Node/PostgreSQL backend implementation and remains available as evidence. The approved current architecture is [Google Sheets one-click](../design/GOOGLE_SHEETS_ONE_CLICK_ARCHITECTURE.md). Historical software acceptance does not establish current deployment or real Google acceptance.

## Overall acceptance

**SOFTWARE COMPLETE — EXTERNAL GOOGLE SETUP BLOCKED**. Real Google deployment and consent are not fabricated. The implemented system is local-first: ESP32 -> verified HTTPS history backend -> Google Sheets. The cloud has no unlock API and no local authentication authority. A transient live LAN health failure is disclosed below; soak evidence is not promoted to an unconditional PASS.

Starting commit: `a1712ca1c1b767787e30f2e964840aa98815eabd`. Ending implementation commit is recorded in `evidence/google_complete/implementation-commit.txt`; this report is committed separately with final evidence. The three pre-existing historical report/evidence changes were excluded.

## EventLog and installation metadata

Schema 3 adds bounded name/role snapshots, identity context, actor/subject IDs and firmware version. Legacy 10-column and pre-Google 12-column segments remain CRC-compatible and are not rewritten. CSV handles Thai UTF-8, quotes, commas and formula-like names. Segment byte limit 8,192 is authoritative; 32 records is a ceiling. Event IDs and original CSV bytes remain stable.

`sl-install` stores a random 128-bit installation ID independent of hardware unit ID and D###### identities. It survives normal reboot and is cleared by the existing physically authorized reset implementation. Corruption disables cloud instead of replacing installation identity. No real reset was performed.

Mode A retains prior bounded recent-history rotation. Pairing first durably enables Mode B protection, which persists after disconnect/reboot. No pending segment is silently evicted. At genuine full capacity new logs can be refused while local access continues. Rotation loss and unrecorded new events are separate. Unrecorded events use persistent reservations of 64, deliberately a conservative upper bound after power loss to limit NVS wear. Corrupt segments are retained, excluded from upload and visible as a fault; later valid segments remain enumerable. Retained-event count is cached to avoid repeated SD traversal in HTTP status.

Access snapshots describe verified identities; claimed IDs are not treated as authenticated. Enrollment/revoke describes the subject. Physical Admin events do not invent an Owner actor. Emergency unlock/relock sequences remain supported. Name changes cannot rewrite old events. RAM-queued events can still be lost on sudden power failure; this implementation preserves the accepted local action/log scheduling policy.

## CloudSync, authorization and transport

CloudSync replaces the obsolete Apps Script configuration/worker path. Default public backend origin/CA are empty; old `sl-cloud` state cannot enable the prototype. Production HTTP controls require current Management authorization, Owner role and actual home-LAN ingress. Google login alone never supplies local Owner authority. Thai Management has connect, confirmed account binding, sync-now scheduling, open Sheet and disconnect; no URL/API-key/Sheet-ID paste fields.

Separate short-lived claim and poll proofs, a persisted pending ingestion key and durable confirmation recover lost replies. Claim proof is HTTPS POST body; ingestion proof is a header. Google access/refresh tokens remain backend-only. CRC-protected dual NVS state slots reject corruption rather than resurrect older enabled state. Polling unchanged metadata does not write NVS repeatedly.

One lower-priority worker receives immutable validated closed-segment bytes. It cannot touch SD, TFT, GPIO, identities or lock permissions. Main EventLog owner pins, revalidates and retires exactly one segment. Cleanup requires verified TLS, HTTP 200, protocol 1 COMMITTED, exact installation/generation/filename/SHA-256 and complete ordered event IDs. PENDING, malformed, partial or wrong receipts never delete data. Failed deletion safely resends. Disconnect/generation change cancels eligibility for an in-flight cleanup.

DNS has a four-second bounded wait with static safe late-callback lifetime. Framework TCP timeout is three seconds and handshake timeout six seconds; nonblocking mbedTLS reads/writes enforce a 20-second absolute transaction limit. Headers/body/response are bounded; redirects/chunked replies are rejected. CA chain, hostname and time verification remain enabled. Main loop does not wait on TLS; worker starts only when STA/time/SD/config/heap are healthy and local lock/Admin/reset state permits it. Scheduling gates are not protection against arbitrary MCU-wide memory corruption.

Retries use 15/30/60/120/240/480/900 seconds plus bounded jitter; safe Retry-After is honored. Only complete verified success resets upload failure backoff. A periodic non-secret status update refreshes loss counters and archive URL; failed status work does not starve uploads.

## Backend, OAuth and Sheets

TypeScript backend supports standard PostgreSQL and PGlite for tests. Numbered migrations create durable ledger, connections, pairings, archives, locks and provisioning-attempt claims. SQL reservations and destination advisory locks serialize fixed-range writes across backend instances. Admission locks keep archive assignment stable. No network call is held in a long SQL transaction.

Google Authorization Code flow uses state, PKCE and nonce. ID token issuer/audience/verified email are checked. Scopes are openid, email and drive.file. Refresh tokens are AES-GCM encrypted with externally supplied key and binding-specific AAD. HTTPS-only Secure/HttpOnly/SameSite browser session cookies handle Google consent; neither Google tokens nor device ingestion key enters browser localStorage. Secrets and provider exception bodies are excluded from logs.

Backend reserves exact rows, checks existing cells, writes RAW bounded contiguous ranges, reads back every canonical cell and only then marks SQL COMMITTED. Lost Google response/crash retries those same ranges. Same ID/different canonical data and manually edited cells conflict; no blind append or silent overwrite. Backend scheduler backoff handles absorbed writer failures. Google API calls and refresh operations have bounded timeouts.

Spreadsheet creation uses durable marker claims plus Drive appProperties lookup. A lost create response cannot trigger another create if lookup temporarily returns no file. An uncertain attempt remains recoverable by lookup; a definitively failed attempt with no file may need operator recovery or fresh pairing. Old remote history is not deleted.

Three automatically provisioned tabs: **ประวัติการเข้าใช้งาน**, **เหตุการณ์ระบบ**, **สถานะระบบ**. Headers are frozen, primary columns readable, technical columns hidden, filtering and warning-only managed-range protection applied. RAW mode prevents formula-like names becoming formulas. One installation archives around 50,000 reserved events; existing batches finish against original destinations. An owner can override warnings/edit their own Sheet, which intentionally stops conflicting delivery.

Exact 24 event columns: occurred_at_local, time_quality, identity_name, identity_id, role, action_label, result, source, event_id, occurred_at_utc, received_at_utc, action, claimed_identity_id, verified_identity_id, identity_context, actor_identity_id, subject_identity_id, uptime_ms, boot_id, segment_id, installation_id, unit_id, firmware_version, event_schema. UTC remains technical truth; Asia/Bangkok is default display. Uptime-only events explicitly show unconfirmed time, distinct from backend receipt time. Missing historical snapshots remain unknown.

## Tests and review

Backend: **83/83 PASS**, including the preserved Phase 1 70 cases, schema/snapshot/RAW, AES-GCM, proof separation, wrong OAuth state, idempotent pairing/revoke, lost create response and delayed marker visibility, scheduler failure signal, native PostgreSQL two-service locks, archive assignment and verified loopback HTTPS. Actual OAuth/request methods are tested against injected expired/revoked/issuer/audience/nonce/scope/quota/deleted errors; escaped pairing keys and URL-based PostgreSQL TLS weakening are rejected. Provider OAuth/Google calls are explicit fakes; native PostgreSQL and HTTPS are real. Crash-process restart tests retain exact reservations.

Integrated host/browser: **21/21 commands PASS** (`host-regression.json`). Tests include production-method registration/reconciliation, stale credential precedence, expiry/wrap, HTTP deadlines, identities/revocation/roles, PIN and network route authorization, GPIO ownership, EventLog/Audit/installation fixtures, browser UI and cloud protocol/state/transport/reset. New focused counts include 41 UI checks, 58 production route checks, 42 CloudSync assertions, 27 transport assertions and 27 reset assertions. Suites use physical dependency fakes and are not physical board results. No invented aggregate assertion total is used for binaries without counters.

SOL reviewed the security/ownership/storage/ACK/OAuth/retention interactions and approved normal deployment only after tests and both builds passed. `sol-review.md` records the gate. Development corrections included an earlier header/compile mismatch, resource CA declaration mismatch, SQL lock handling, grouped Google ranges, polling wear, status scheduling, callback lifetime and backend recovery. Failed iterations were corrected, not counted as PASS.

## Resources and deployed baseline

Unchanged application slot: **1,310,720 bytes**. Actual default deployment binary **1,115,824 bytes**, headroom **194,896 bytes**, static RAM **107,336 bytes**. LTO removes unreachable cloud transport while origin is empty; this smaller binary does not prove enabled-cloud size.

Full-cloud resource build with placeholder `.invalid` origin (NOT flashed): actual binary **1,271,792 bytes**, headroom **38,928 bytes**, static RAM **108,476 bytes**. Growth over accepted pre-Google binary is **14,576 bytes**, under the preferred 25 KB target. ELF program-size output differs from binary length; the headroom above uses actual binary bytes. Deployment hashes are in `sol-review.md`. No partition/layout or large framework change was made.

Preflash current authority: ko/D000001 Owner ACTIVE, hi/D000002 USER ACTIVE, one Owner/two identities/two verifiers, configured, healthy PIN store, Wi-Fi connected, 5,000 ms unlock duration, calibrated touch, healthy SD, GPIO22 HIGH/locked. D000002 was ACTIVE at the actual baseline; older expected revoked evidence was not substituted.

Final live deployment, persistence comparisons and soak measurements are appended after collection below. Chosen PIN value/verification material were never printed; actual physical PIN entry remains distinct from store health.

Implementation commit: `9217f0c75cdb144a07ac2d451f9150e886f621da`.

## Collected live and staging evidence

COM6 upload PASS: 1,115,824 bytes written and hash verified; normal EN reboot only. `postflash.txt` reports GPIO22 locked, configured state/Owner/two identities/two verifiers preserved, PIN store healthy, Wi-Fi connected, 5-second duration, touch calibration/SD healthy, installation store healthy and all temporary authorization zero. `persistence-comparison.json` passes every recorded comparison. The oldest pre-existing SD segment remains present; additional boot/denial logs are expected.

Live negative API checks **21/21 PASS**: all seven cloud routes reject invalid Management authority, malformed Access/Enrollment reject without unlock, obsolete mutation handlers return 404, Owner bootstrap returns 410, slow body terminates around 5.06 seconds and health returns 200 afterward. The first fixture followed generic retired-GET redirects to Setup and incorrectly expected 404; its failed expectation is retained in `live-http-first-expectation.json`. Corrected fixture disables redirect following and verifies both GET fallback and unavailable POST mutation. No production change was needed for that test correction.

Canonical hostname initializes with `MDNS: OK` on the board. A Windows multicast/unicast-response probe timed out; Windows .local resolution remains DEFERRED, distinct from working numeric LAN HTTP. No phone browser PASS is claimed.

Backend soak **PASS**, 1,200.135 seconds, 957 batches/2,871 events, 4,594 HTTPS requests, 766 injected offline/quota/lost-write/partial-write recoveries and 47 rejected invalid-TLS probes. Exact row count and all COMMITTED receipts were checked; no duplicate delivery. Maximum staging health latency 17 ms. Google requester is explicitly fake. This process exercised the tested history writer/HTTPS/SQL path before the final pairing-parser/DB-configuration hardening; those later changes passed the final 83-test suite. It does not measure ESP32 TLS memory or real Google availability.

Board soak: **DEGRADED / fixture FAIL**, 1,200 seconds, 40 samples, 38 successful health probes and two transient URLErrors at 272.1 and 972.8 seconds. Every Serial sample still reported locked/configured/STA connected; later HTTP samples recovered. No panic/watchdog evidence appeared. The fixture retained only exception class, so it cannot distinguish TCP loss from device HTTP availability. Current proxy inspection found no configured proxy. This is an unresolved reliability observation, not a claim of uninterrupted network service or proof that the Google changes caused it (cloud worker was disabled).

Follow-up direct-LAN observation: **PASS**, 300.45 seconds, 251 requests, zero failures, maximum health latency 2,272 ms. It bypasses proxy settings and records full non-secret error detail on failure. No broad network refactor was introduced based on an unproven cause. Final real Google/active ESP32 cloud acceptance must include a further LAN responsiveness soak; investigate repeated connection failures before declaring uninterrupted network reliability.

Board soak observed free heap **133,656–139,856 bytes**, minimum heap **105,768**, largest free block **77,812–86,004**. Backend RSS rose from 67,641,344 to 99,872,768 bytes while its deliberate in-memory Google fixture accumulated 2,871 rows; this is not an ESP32 heap measurement or proof of an unbounded production leak.

Final normal reboot **PASS** (`final-reboot.txt`, `final-persistence.json`): same installation ID/unit ID, same ko/hi metadata and one Owner/two verifiers, configured/PIN-store/Wi-Fi/duration/calibration/SD preserved, GPIO22 locked, temporary authorization zero, EventLog enabled without fault/full/loss. Oldest segment is unchanged and retained count is 17 after legitimate boot/denial events. Cloud remains safely disabled/external setup required. Post-reboot system diagnostic free heap **138,216**, minimum heap at that diagnostic **119,024**, largest block **86,004**; later Auth diagnostic reported minimum **109,692**. Final LAN health returned 200. These are distinct points in time, not interchangeable best-case figures.

The first final-reboot health probe also timed out during Python `socket.connect` after five seconds. A following direct curl returned 200 in 85.8 ms (TCP connect 54.2 ms), and three pings had zero loss. This strengthens the need to investigate intermittent idle LAN/TCP availability before claiming uninterrupted operation; it does not establish a cloud-worker regression because cloud stayed disabled. `final-health.json` preserves both outcomes.

Minimum remaining acceptance: configure the owned backend/OAuth/CA, consent through Owner Management, verify real Google provisioning/upload/readback/exact retirement/lost ACK/reconnect, then run an active-cloud ESP32 resource/LAN soak. Perform one authorized Owner browser unlock/relock and physical magnet/PIN interaction when the user is present. Do not Factory Reset or recreate Owner for these checks.

## External acceptance and remaining limits

No usable existing OAuth client, owned production endpoint/domain or account consent was available. No paid cloud resource, Google Sheet or account configuration was invented/created. Production code and deployment instructions are complete, but actual Google OAuth/Sheets end-to-end and active ESP32 TLS heap/recovery remain **EXTERNAL PENDING**. See `backend/DEPLOYMENT.md`.

One human action bundle: supply an owned HTTPS backend deployment with PostgreSQL and OAuth web-client/consent configuration, then use the existing Owner Google connection UI to consent. The public origin/root CA must be supplied to the reviewed firmware build. Final real-Google acceptance must exercise provisioning, fixed-range write/readback, exact ACK retirement, lost ACK/no duplicates and revoke/reconnect. No token/password needs to be pasted into SmartLock UI.

Factory Reset integration is host-tested only; remote Sheets are never deleted by reset. Disconnect retains pending local history and removes backend token access, but Google account-level grant revocation is a separate account action. Abandoned reset-era backend grants must be revoked separately.

First backend binding uses trust on first use of the high-entropy installation ID; the backend does not attest physical hardware or independently verify a local Owner credential. Thereafter the existing ingestion key is required to reconnect. Publishing an unpaired installation ID before first connection permits a cloud-registration squatting/conflict scenario requiring backend operator recovery, without granting local door authority. Keep staging OAuth test users restricted and do not treat this backend as a general device-attestation service.

The last-sync display timestamp is updated after verified ACK in RAM; a later metadata state save also persists it. After reboot it can be older/unknown, although exact ACK/retirement and durable backend history remain correct. This status-display limitation does not authorize deletion or duplicate delivery.

Changed implementation files are enumerated by `git show --stat 9217f0c`; they cover 68 scoped backend/source/test/script files. Historical unrelated modifications remain outside these commits. Task-specific PostgreSQL staging was stopped after successful tests/soak; no Google service or public deployment was left running.

New console evidence was normalized from Windows redirection UTF-16 where applicable to UTF-8, with insignificant trailing console whitespace removed. Commands, failures, assertions and outcomes are retained; historical evidence was not changed.

Physical magnet movement and authorized Owner-browser/PIN interaction remain **PHYSICAL PENDING**. Software GPIO/timer tests and Serial locked state are not claims of magnet motion. Soaks are bounded staging observations, not proof of multi-day production stability.
