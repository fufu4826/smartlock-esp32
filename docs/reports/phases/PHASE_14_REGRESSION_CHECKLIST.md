# Phase 14 regression checklist (items carried forward)

Execution policy: user explicitly authorized automatic development through Phase 14 on 2026-09-26. Software acceptance and physical acceptance are separate. Nonblocking human-only checks are consolidated here. Destructive reset/restore and real Google deployment remain explicit checkpoints.

Latest priority override: Google deployment is paused. Prepare one real Phone #2 test now (see LAN_PHONE2_CHECKPOINT.md); Phone #3 remains deferred. AP is Owner setup/recovery, LAN is normal multi-user operation.

- [ ] New LAN policy: successful LAN verification closes AP after response grace; scan preserves STA-only; sustained outage restores protected recovery; non-Owner AP Access/Management denied; AP identity enrollment always denied.
- [ ] Phone #2: single identity name/role selection, one-time QR, independent credential, physical Access/relock, selected-device revoke, fresh Access denial, Owner still valid. Reboot preserves revocation; no separate User disable operation exists in the single-identity model.

- [ ] Phase 9 bad-password candidate: clear failure, protected AP stays accessible, known-good saved STA reconnects after reboot, same Owner/Device records, magnet remains LOCKED. Isolated software tests do not count as physical observation.
- [ ] Phase 10 protected Recovery AP after repeated saved-STA failure; BOOT hold recovery; authorized correction; no database wipe or unlock.
- [ ] Current phone canonical-host access after each final firmware reboot, AP and LAN; keep the existing Owner Chrome/profile.
- [ ] Phase 11 event UI/export and physical unlock/relock event pairing; timestamp quality accurately shown when offline.
- [ ] Phase 12 real configured Google upload, retry with lost ACK creates no duplicate, acknowledged segment cleanup preserves authorization data.
- [ ] Phase 13 authorized backup/restore/OTA physical regression; destructive real restore/reset requires explicit authorization.
- [ ] Final boot: physical magnet energized/LOCKED; no startup glitch, watchdog, brownout or unintended unlock.
- [ ] Thai Setup on a clean state only after separately authorized reset; register exactly one Owner identity D000001.
- [ ] Canonical hostname on AP and home LAN with the same Owner Chrome/profile; no handoff or repeated registration.
- [ ] Single tap Access QR; authorized physical unlock, timed relock, consumed URL rejected without another unlock.
- [ ] Hold approximately 1.5 seconds for Management; prior Access invalidated; all eight Thai sections reachable; Management never unlocks.
- [ ] Management-to-reset hold: red screen without QR; No returns to Access; Yes requires separate OK. Actual reset requires explicit authorization and verified reset completion.
- [ ] Setup blue, Access white, Management yellow, reset red; every displayed QR has a white background.
- [ ] Safe microSD fault test only when it cannot corrupt the live database; authorization fails closed while network recovery remains safe.
- [ ] Reboot persistence: one authoritative Owner record, no unintended Device duplicate, revocation and saved Wi-Fi preserved.
- [ ] Logs: new actions use empty legacy user_id and D###### in device_id; historical User/Device events remain readable, denial does not expose secrets, unlock/relock events paired, Thai export usable.
- [ ] Final GPIO22 audit and physical LOCKED observation after all tests; no Bluetooth.

- [ ] Repeat Phase 6 enrollment with two additional independent physical phones. Confirm each phone receives a separate browser credential, one-time enrollment QR is consumed, expired and replayed QR URLs fail, revoking Phone #2 leaves Owner/Phone #3 active, state survives reboot, and GPIO22 remains LOCKED throughout. This physical multi-phone test was deferred by user request during Phase 6; browser/storage-context and API substitutes do not count as this physical confirmation.
- [x] Physically reload consumed Access URL: rejected with no second unlock, user canonical clean acceptance on 2026-09-26.

- [ ] Reboot after a genuinely changed DHCP LAN address; verify canonical hostname and same Owner credential without additional Device.
- [ ] Investigate Windows OS `.local` resolution failing after reboot despite correct direct mDNS answer; repeat Windows/Android AP and STA resolver regression.

- [ ] Repeat explicitly authorized full Factory Reset after RAM-only Wi-Fi change; verify confirmed sl-sta/sl-net configuration and legacy SDK-persisted Wi-Fi settings are cleared, Users/Devices are removed, Setup returns, and lock behavior remains safe. No new destructive reset was performed during Phase9 work.

- [ ] Single-identity migration: existing Owner phone still unlocks/relocks without Setup or re-registration. Exactly one D000001 Owner and original verifier survive normal reboot; Wi-Fi/hostname/calibration retained. Complete this before Phone #2.
- [ ] Simplified Phone #2: assigned unique name and role, web-only QR, no second name/password prompt; own credential; physical Access/relock; revoke -> fresh Access denied; Owner still works; reboot persists.


### 2026-09-26 real Phone #2 results

- [x] User confirmed existing Owner valid; new boom identity enrolled through web QR without second name/device prompt and received an independent credential.
- [x] Real Phone #2 fresh Access QR unlocked and automatically relocked (user-confirmed physical PASS).
- [ ] Owner revokes D000002 / boom only; Phone #2 fresh Access denied and physical magnet remains LOCKED.
- [ ] Existing Owner fresh Access unlock/relock after boom revocation; D000003 / Root unaffected.
- [ ] Verify revoked state persists after an appropriate normal reboot following the physical checkpoint.

Phone #3 remains deferred. Seeing D000003 / Root in the database is not evidence of the deferred physical Phone #3 test. Google deployment remains paused.

### Revoke user PASS with unresolved live discrepancy

User reported all final Phone #2 revoke and Owner-after-revoke physical checks PASS. Read-only USB before and after a normal reboot instead showed D000002/boom ACTIVE; boot counts active=3 revoked=0. Physical observations are recorded, but persistent revocation is DEFERRED by explicit user decision; recheck in Phase 14. Further investigation is paused. No reset or identity mutation performed. Google paused; Phone #3 deferred.

### Latest user Phase 14 policy

All following items remain pending; historical physical observations do not complete reboot persistence:

1. **DEFERRED**: boom / D000002 revocation persistence after reboot.
2. **DEFERRED**: Phone #3 physical enrollment/revoke.
3. **PAUSED**: live Google Sheets deployment/synchronization.
4. Windows hostname/HTTP reliability (one successful IP probe is not a full regression).
5. Changed DHCP address with the same canonical browser origin.
6. Final existing Owner Access unlock/timed relock/replay physical regression.
7. Final Phone #2 enrollment/access/revoke regression.
8. Factory Reset physical flow, only at an explicitly authorized destructive checkpoint.
9. Recovery AP physical restrictions and safe return to LAN.
10. Backup/restore/OTA physical checks where safe; production restore remains a destructive checkpoint; authenticated OTA upload is implemented, with controlled Owner-browser transport/partition-switch acceptance pending.


### Phase 13 maintenance implementation continuation

- [ ] Existing Owner browser: download verified current firmware, perform authenticated OTA with that same image, confirm inactive-to-active partition switch and completed result after locked reboot. Preserve all identities/verifiers, saved STA, canonical origin and calibration.
- [ ] Physical real restore and interruption test, only after an explicitly reviewed safe/destructive checkpoint. Host generation fault fixtures are not physical SD/NVS power-loss evidence.
- [ ] Hardware interruption at restore SD/NVS/journal boundaries: old complete state or validated restored state, never mixed authority, GPIO22 locked.

D000004/Ki was present in the Phase 13 pre-flash read-only baseline; preserve it. Its presence is not a Phone #3 physical acceptance claim.

### User closeout decision (2026-09-26)

Remaining Phase 14 physical checks above are **USER WAIVED / NOT TESTED**. Phase 14 software/runtime checkpoint is **ACCEPTED FOR CONTINUED DEVELOPMENT**. Unchecked boxes are not physical PASS claims; see PHASE_14_REPORT.md. The separately authorized Physical Admin PIN feature follows. Factory Reset and Google deployment remain postponed.
