# Phase 11 — bounded local events, time and export

Status: IMPLEMENTATION PASS / SOFTWARE ACCEPTANCE PASS / PHYSICAL CHECK DEFERRED TO FINAL REGRESSION.

## Architecture and review

Sol specified the schema, sensitive-data exclusions, clock trust, authorization integration and storage-failure behavior; reviewed Luna's EventLog and Thai Logs UI, repaired integration issues and made the phase decision. Luna's isolated fake-SD tests do not approve security or physical acceptance.

Event records contain a random boot nonce plus sequence event_id, timestamp, explicit NTP/UPTIME quality, validated user/device IDs, enum-only action/result, AP/AP_STA mode, uptime_ms and CRC32. Unknown wall time is empty rather than invented. NTP is an audit clock only; authentication/session expiry remains monotonic. No browser-provided clock is accepted. SNTP is unauthenticated time, not a security authority; sync older than one day is treated as unknown.

The queue holds 16 events. SD outbox is capped at 64 segments, at most 32 records/8192 bytes each. Writes append one event per loop drain; historical files are not rewritten. Full/failed storage increments visible drop counters and retains existing unsynchronized data. Malformed, partial or duplicate-ID segments are retained and flagged; bounded readers reject them. Only a caller that has validated an explicit cloud ACK may use exact selected-segment deletion. CSV export never invokes deletion. Authorization data is outside the outbox and never touched by logging.

Hooks cover boot, authorized access/unlock/timed relock, access denial, Management login, User creation, enrollment, revocation, network changes and recovery. Denials with untrusted identity claims use empty IDs to avoid recording a false authenticated identity. Successful access currently retains the Phase 7 Owner-only policy and records U000001 plus the verified Device. No role expansion occurred here.

Management-only POST endpoints provide status and export of the oldest closed segment. The Thai UI accurately labels this as oldest-segment export, not pagination. Export is bounded to 8192 bytes; repeat export preserves records. USB `DIAG_AUDIT` is a fixed local diagnostic that seals and validates one log segment, never reads credentials, modifies authorization, resets or unlocks.

## Verification

- Final build PASS: RAM 117568/327680 bytes; flash 1060457/1310720 bytes. COM6 upload PASS in 71.13 seconds with image hash verified. A subsequent final-source build also PASS.
- Two monitored reboots: GPIO22 readback LOCKED, timer OK, configured/Owner YES, SD/session/DB PASS, users=1 devices=1 active=1 revoked=0; saved STA reconnect; AP/DNS/mDNS/HTTP available; AUDIT READY. Heap free 132860, minimum 127496. No watchdog/brownout/boot-loop/unexpected unlock observed.
- USB audit probe: enabled=1, fault=0, full=0, queue=0, dropped=0, NTP=1. CRC-validated BOOT/NETWORK_CHANGED records with identical stable event IDs survived reboot; segment count increased from 2 to 4 without deleting history.
- Canonical-host HTTP tests passed: Phase 8 protected APIs, Phase 7 invalid session/credential rejection, and unauthorized logs/status/export all denied. A direct multicast probe timed out on one attempt while canonical HTTP remained available; intermittent resolver behavior remains final regression.
- Native fake-SD tests PASS for append/seal/read/delete gating, queue and segment limits, reboot persistence, invalid IDs, CRC/duplicate-ID corruption, partial write retention, and unavailable SD. These never touch real SD/auth data.
- Thai UTF-8 audit, embedded JS syntax and git diff --check PASS. Sol reviewed final hooks: logging does not decide access, only LockController writes GPIO22, and the AccessController unlock gate is unchanged.

## Limits and deferred observations

No valid unlock, real enrollment/revocation, destructive reset, card removal or authenticated human export click was performed for this phase. Physical event pairing and Thai visual confirmation are deferred to Phase 14. Power loss can lose RAM-queued or partially appended events; damaged segments are retained rather than silently repaired or acknowledged. At the bounded outbox limit, new events may be dropped with counters, while local authorization remains independent. Long-term history requires configured cloud sync in Phase 12.
