# Phase 10 — protected network and physical recovery

Status: IMPLEMENTATION PASS / SOFTWARE ACCEPTANCE PASS / PHYSICAL CHECK DEFERRED TO FINAL REGRESSION.

## Plan and ownership

Sol owns recovery timing, authentication preservation, BOOT semantics and fail-closed behavior. Luna supplied isolated networking tests; Sol independently reran and reviewed them. No real factory reset, bad-password submission, SD/NVS deletion, Owner replacement or valid unlock request is used for this phase.

## Implementation

- The normal protected AP remains available during candidate failure. After a continuous failure interval of 60 seconds, the AP is renamed `SmartLock-Recovery-XXXX`, retaining its existing WPA2 password. The IP and canonical origin are preserved. Recovery never opens an unauthenticated configuration path.
- Every 60 seconds of failed STA state, retry only the last confirmed StaSecrets record. These retry connections cannot save or replace configuration. A missing confirmed record starts no blank association.
- Hold BOOT for five seconds after firmware startup to request recovery. The path first forces LOCKED and invalidates Access sessions, then requests the protected AP. It is nonblocking and fires once per hold. Holding GPIO0 across reset may select the ESP32 ROM loader, so runtime BOOT hold is the practical physical procedure. A short BOOT press during firmware startup retains the existing touch recalibration behavior.
- The stronger touchscreen factory-reset flow is preserved; network recovery never calls it and never wipes the database. Correcting the network still requires an authenticated Management session. A normal reboot restores the normal AP name and reconnects with confirmed STA settings.
- Authenticated Network status reports recovery mode, rendered with Thai text.

## Memory repair and review

The initial build exceeded static DRAM by 40 bytes. Sol replaced only FirstOwnerSetup's temporary validation arrays with value-initialized `nothrow` heap scratch owned by `unique_ptr`. Both allocation failures return false. The 64-record capacities and existing validation rules are unchanged; buffers are freed after validation. No database schema or content changes.

## Isolated acceptance

`python tests/network/run_network_manager_tests.py`: 89 assertions across seven scenarios PASS. Includes Phase 9 transaction checks, recovery exactly after the defined failure interval, preservation of the AP password, confirmed-record-only retries, no retry writeback and missing saved credentials. These tests compile production NetworkManager.cpp with isolated fakes and no database dependency. They do not establish physical AP reception, BOOT behavior or magnet observation.

Sol reviewed GPIO ownership: only LockController writes GPIO22 and only the authorized AccessController path requests unlock. Recovery code contains no database or unlock call.

## Deferred physical acceptance

Protected Recovery AP reception, BOOT hold, correction using the existing Owner browser, physical magnet state through failure/reconnect, and any explicitly authorized destructive reset remain on the Phase 14 checklist. No physical PASS is inferred.


## Final runtime and Sol closeout

Final build/upload COM6 PASS (138.56 seconds, image hash verified). Boot Serial: LOCKED; timer OK; configured/Owner YES; SD OK; sessions PASS; database PASS users=1 devices=1 active=1 revoked=0; normal AP/DNS/mDNS/HTTP OK; saved STA reconnected to 192.168.1.179. Heap free 134112, minimum 133812 bytes. No watchdog/brownout/boot loop or unexpected unlock observed in the 35-second monitored boot.

Read-only canonical probe PASS: direct mDNS advertises 192.168.1.179, HTTP with canonical Host responds, and Windows OS hostname lookup also resolved correctly on this boot. Its earlier intermittent failure remains a repeat-regression item. Phase 8 protected API and Phase 7 negative session/credential checks PASS; Thai UTF-8 audit and JS syntax PASS. Sol independently reran 89 native assertions and reviewed final diff; git diff --check PASS.

EventLog source was being prepared separately and was unused/dead-stripped in this firmware; no Phase 11 log integration or acceptance is claimed here. Production Phase 10 changes are committed separately before integrating logging. Final decision: implementation and software acceptance PASS; all human-only recovery observations remain deferred. Continue to Phase 11.
