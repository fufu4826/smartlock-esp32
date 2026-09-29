# Phase 14 — final regression

Baseline: `7f6c0e80072a9fb5983edf5be7c0e5d76c32ecca`; initial working tree clean. Phase 13 evidence accepted without repeating suites. No firmware changes, reset, flash or new features.

## LIVE BOARD PASS

- Read-only COM6 DIAG_AUTH: database valid; four identities/four verifiers; exactly one Owner; Owner verifier preserved. D000001/gugy OWNER ACTIVE; D000002/boom USER ACTIVE; Root and Ki unchanged.
- Numeric-IP `/api/status`: configured, ready, canonical hostname smartlock-04225a0ff0a4.local. Identity reads confirm SD available; current LAN responds. Saved STA/calibration and canonical initialization have accepted Phase 13 boot evidence; fresh boot verification follows physical checkpoint.
- Artifact unchanged: 1,276,288 bytes; SHA-256 7daf74158a2f3d9cdad2d062d382da0dbbed345ce93150d8c3a41b79ab8ab322; slot 1,310,720 bytes; headroom 34,432 bytes.
- Quick source audit: lock pin writes only LockController; unlock caller only AccessController. Maintenance barrier gates Access; normal boot locks. No emergency unlock implemented. Access/Management/Enrollment normal URLs use CanonicalOrigin, not historical numeric IP.
- GPIO22 HIGH is accepted Phase 13 evidence, not a fresh electrical measurement in this pre-check. Physical confirmation remains required.

## PHYSICAL PASS

No new Phase 14 physical PASS yet.

## USER WAIVED / NOT TESTED

- Existing Owner Access/unlock/timed relock.
- D000002 revoke, denied Access, normal reboot, authoritative REVOKED persistence, denial after reboot, Owner unaffected.
- Fresh encrypted current-state backup and controlled same-state restore activation; post-boot generation/journal/config/auth/STA/calibration/lock verification. Existing UI has no editable harmless display setting; no identity or Wi-Fi change will be used as a restore fixture.
- Controlled current-image OTA, partition switch and preservation checks; final Owner Access.
- Recovery AP healthy-state shutdown and sustained-failure/reconnect physical behavior. Router shutdown is not requested now.

## DEFERRED

Windows .local resolution; Phone #3; changed-DHCP physical test unless naturally observed; Google Sheets; ADMIN PIN; final Factory Reset cycle.

## DEFECTS FOUND/FIXED

None established. D000002 is currently ACTIVE; prior revocation discrepancy is unresolved until controlled revoke/reboot evidence. No source change made.

Overall: ACCEPTED FOR CONTINUED DEVELOPMENT by explicit user decision. Every listed unperformed physical check is USER WAIVED / NOT TESTED. No physical restore, controlled OTA, or D000002 reboot revocation PASS is claimed. User authorized the separate Physical Admin PIN feature next; no Factory Reset or Google deployment was performed.
