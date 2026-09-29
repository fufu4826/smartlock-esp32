# Phase 7 — Owner Access QR over home LAN

Status: PASS. The user explicitly reported physical acceptance on 2026-09-26. Phase 6 multi-device acceptance remains paused/deferred by user request.

## Physical evidence

Home STA connection, AP-to-LAN transition, automatic Owner handoff, LAN Access QR reachability, Owner recognition, physical magnet release on a fresh Access QR, and timed re-energizing all passed. No repeated manual Owner verification was required. Physical replay testing was not explicitly included in the latest result; replay rejection is covered by source review and the session self-test, with physical replay retained for final regression.

## Sol final security review

- Access tokens use 256 random bits, expire after 30 seconds, and are single-use. Boot self-test checks replacement, wrong type, expiry, invalidation, consumption, and repeat consumption rejection.
- AccessController checks healthy configured state, duration bounds, logical database integrity, salted device verifier, ACTIVE Device, and ACTIVE Owner. Missing/invalid credentials and revoked/inactive records cannot authorize unlock.
- Token consumption is rechecked immediately before the sole production call to LockController.unlock(). Replay/reload cannot extend the timer; already-unlocked requests do not restart it.
- Production GPIO22 writes are confined to LockController, which preloads LOCKED at boot. The independent ESP one-shot timer relocks, with a loop fallback. The separate maintenance reset tool only drives LOCKED and is not production firmware.
- Authenticated Owner Management prepares a random five-minute single-use bootstrap bound to the existing ACTIVE Owner device and current LAN IP. LAN completion consumes before persistence and adds an ACTIVE Device and salted verifier associated with existing U000001. It does not create another Owner. Duplicate IDs and reused credentials are rejected by stores.
- The LAN page performs the handoff automatically without a second registration form. Permanent credentials stay in browser storage; only salted verifiers are persisted. The old popup/postMessage routes are unregistered. Manual verification is retained only as recovery.
- Confirmed STA uses alternating integrity-checked NVS slots. Association and nonzero IP precede persistence. The protected AP remains available throughout attempts and failures. Saved STA reconnect after reboot was observed.
- Serial review found non-secret states, counts, and IP only; no password, passphrase, credential, or full temporary token logging.

## Verification and limits

JavaScript syntax and firmware build passed. Final closeout flash, Serial, database counts, heap, and LAN negative API results are recorded below. Automated probes use synthetic tokens and do not authorize unlock.

Consumption before persistence can leave an orphan verifier after interrupted writes; it cannot unlock without an ACTIVE Device record. A lost bootstrap success response may require a fresh authenticated link. HTTP and localStorage remain the local-network transport design, not TLS protection.

Physical Phone #2/#3 enrollment and revoke validation remains DEFERRED and is tracked in PHASE_14_REGRESSION_CHECKLIST.md. No physical multi-phone result is claimed.

Proceed to Phase 8 after closeout verification and commit. Preserve the Access path, U000001, saved configuration, AP recovery, and physical reset interaction.

## Closeout verification on 2026-09-26

- JavaScript: seven embedded pages passed syntax checks.
- Build: PASS; RAM 122,584/327,680 bytes (37.4%); flash 979,025/1,310,720 bytes (74.7%).
- Flash COM6: PASS; image hash verified. Standard application upload was used; no NVS erase or reset tool was invoked in this closeout.
- Boot: LOCKED; lock timer OK; TFT/touch OK; calibration loaded; SD OK; session self-test PASS; AP/DNS/HTTP OK; free heap 139,060, minimum 138,768 bytes.
- Unexpected current state: Configured NO, Owner exists NO. No saved STA reconnect was attempted in this setup state. The prior user-reported physical PASS remains evidence, but current configured database/STA preservation cannot be verified yet. User clarification/setup checkpoint is pending. Closeout commit and Phase 8 have not started.

## User clarification and reordered priority

The user confirmed the clean state was an intentional factory reset after the physical PASS. Configured NO / Owner exists NO is expected, not a persistence regression. Setup is deferred until visual approval of the Thai web UI. The immediate task is presentation-only localization; Phase 7 configured persistence recheck/commit and Phase 8 dashboard work resume afterward.

## Canonical clean acceptance supersedes legacy normal handoff

2026-09-26: User intentionally reset and repeated Setup using `http://smartlock-04225a0ff0a4.local` in one Chrome/profile. Normal AP-to-STA transition, unchanged browser credential, immediate Owner Access, physical unlock, timed relock, and consumed-URL replay rejection PASS. No handoff/second registration was used. Post-reset closeout build/COM6 flash PASS; boot SD database integrity PASS with exactly one User/one ACTIVE Device (U000001 Owner/D000001), saved STA reconnect, canonical mDNS and LOCKED start PASS. See CANONICAL_OWNER_ORIGIN_REPORT.md for full evidence, Windows OS resolver limitation, and deferred DHCP/multi-phone regressions. Historical descriptions of automatic normal-flow bootstrap above are superseded; bootstrap is explicit numeric-origin recovery only. Phase6A verified normal network transition PASS; Phase7 Owner Access PASS.
