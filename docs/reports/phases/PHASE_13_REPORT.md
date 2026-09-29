# Phase 13 - atomic restore and authenticated OTA

Date: 2026-09-26. Continuation of checkpoint `df7b61f`.
Sol owns architecture, authorization, storage/recovery, GPIO22 safeguards and final acceptance. Luna implemented bounded Thai UI and isolated test harnesses; Sol independently reviewed their results and limits.

## Acceptance status

- IMPLEMENTATION / SOFTWARE CHECKPOINT: PASS. Encrypted backup/validation, atomic restore activation/recovery, streamed authenticated OTA and Thai maintenance UI independently reviewed by Sol.
- HOST PASS: isolated restore/storage, OTA, privilege, session, browser and existing regressions below.
- ON-BOARD NON-DESTRUCTIVE CHECKPOINT: PASS; accepted evidence recorded below.
- PHYSICAL: no new Phase 13 physical restore/OTA PASS is claimed.
- PENDING: physical restore activation acceptance; controlled real OTA upload acceptance. Neither was performed on production for this checkpoint.
- DEFERRED: Windows .local resolution; D000002 revocation persistence; Phone #3; Google Sheets deployment.

Full physical maintenance acceptance remains open. The implementation is not an assertion that card power-loss behavior or production restore has been physically validated.

## Production state and preservation policy

Read-only pre-flash USB evidence is in `evidence/phase13/baseline-auth.txt`: exactly one active Owner D000001/gugy, four identities/verifiers, original Owner verifier preserved. D000002/boom, D000003/Root and D000004/Ki are present. Ki was already present before this flash and is preserved. No identity was added, revoked, removed or recreated by these maintenance tests.

No Factory Reset, credential replacement, NVS global clearing, partition resizing, Wi-Fi reconfiguration, live restore, Google deployment or Phone #3 test was performed. Legacy authorization files remain intact on ordinary boot. Existing Owner browser storage is unchanged.

## Real production OTA capacity

Read-only COM6 dumps before OTA implementation: `evidence/phase13/production-partitions.bin` and `production-otadata.bin`. The OTA selector had valid sequence 1/CRC 0x4743989a; running slot app0 was also scheduled for runtime confirmation.

| Partition | Offset | Capacity |
|---|---:|---:|
| app0, current | 0x10000 | 1,310,720 bytes |
| app1, inactive | 0x150000 | 1,310,720 bytes |
| NVS | 0x9000 | 20,480 bytes |
| otadata | 0xe000 | 8,192 bytes |

Checkpoint binary: 1,244,128 bytes; original inactive headroom 66,592 bytes. Final firmware binary: **1,276,288 bytes**, leaving **34,432 bytes OTA headroom** in the 1,310,720-byte inactive slot. Program usage is 1,269,713 bytes (96.9%); static RAM is 109,108 bytes (33.3%). Clean full build and final reviewed rebuild passed. The generated partition table matches the production layout; no repartitioning or full-chip erase was performed. Runtime confirms app0 active and app1 inactive.

## Restore activation and cross-store atomicity

`POST /api/system/restore/activate` is Owner-authenticated Management, lock already HIGH, rate limited, bounded exact form content, with passphrase and encrypted package. It decrypts and repeats full validation; a prior UI validation does not confer authority. The existing Owner name, D000001 credential verifier and admin verifier must remain identical.

The commit is a **single NVS generation selector**, not sequential independent live file replacements:

1. Freeze Access and authorization/configuration/reset actions with a maintenance barrier; invalidate old Access sessions.
2. Capture a fresh complete pre-restore anchor. Decrypt/validate the candidate for this board.
3. Stage immutable SD directories `/smartlock/db/gXXXXXXXX/` containing identities, auth and committed-mode marker. AtomicFileStore envelopes verify write/flush/readback/rename.
4. Store each generation's complete configuration, AP/STA secrets and touch calibration in its NVS bundle (`sl-generation/gXXXXXXXX`), with generation ID and CRC. Verify SD plus NVS as one complete snapshot.
5. Durably journal Prepared, then Started. Atomically commit `sl-generation/active` to the candidate ID; this selects BOTH SD data and NVS fields.
6. Write Activated; reboot locked. Boot validates the selected generation before any Access authority becomes available.
7. After database/configuration/Owner/calibration/AP/canonical/HTTP initialization is safe, write Finalized and release the barrier.

All normal auth/identity/config/AP/STA/calibration consumers use the selected generation. Ordinary installations without a selector retain existing legacy paths and NVS state. Normal updates after finalization are allowed; boot validates actual current data rather than reverting to the original staged CRC.

### Recovery decisions

- None/Finalized: validate current selected state; never resurrect an arbitrary earlier generation.
- Prepared: cancel without rolling back subsequent legitimate changes; preserve current selection.
- Started/Activated: accept the complete strict candidate; otherwise select only this transaction's verified previous anchor, record RolledBack, initialize safely, then finalize.
- RolledBack: initialize that specific complete anchor; finalize only after safe initialization.
- Bad journal/selector, both generations invalid, or failed finalization: fail closed, retain the maintenance barrier and locked output. No default/empty authorization repair is used.

The previous generation is retained. Automatic cleanup of old generation directories/bundles is intentionally not performed. Storage exhaustion fails staging safely; it does not erase rollback data. Hardware NVS atomic-record durability and SD flush/rename persistence are assumptions of the host model and still require a separately authorized hardware interruption regression.

Strict package validation covers authenticated GCM/AAD, same board, framing/CRC/config/secret flags, exactly one ACTIVE D000001 OWNER, unique IDs/canonical names, valid roles/statuses, nonempty verifiers and complete identity/auth one-to-one relationships. Orphan verifiers block restore rather than being silently repaired.

## Authenticated OTA implementation

Owner-only LAN Management start additionally requires the active D000001 browser credential. ADMIN/USER/GUEST, expired/revoked authority, Recovery AP, an active unlock timer, and unavailable/insufficient inactive capacity are rejected. OTA cannot call unlock. The same maintenance barrier blocks Access/reset/config changes for upload and pending boot verification.

- `ota/current`: authenticated Owner download of the running image after SDK image verification; read from flash in 1 KB blocks. This provides a known running image for the controlled test without exposing SD/NVS secrets.
- `ota/prepare`: reports real inactive capacity and lock state.
- `ota/start`: bounded form; verifies Owner/session/credential, snapshots the preservation digest, starts the inactive partition updater, and issues random 256-bit ticket and read-only receipt capability.
- `ota/chunk`: individual form requests contain at most 1,024 decoded image bytes. The body limit is 2,300 bytes before parsing; hex must be lowercase and offsets exactly sequential. This is application-level streaming, not HTTP Transfer-Encoding chunking or whole-image RAM buffering.
- Header checks include ESP magic, segment count, SPI mode, ESP32 chip ID. The SDK validates the full completed image; SHA-256 tracks exact accepted bytes.
- Invalid size/header/order/duplicate/final chunk, short updater write, incomplete finish, revoked/expired authority or idle timeout abort the inactive update. Idle limit 30 seconds; total limit 300 seconds. A mid-request disconnect cannot program a partial HTTP body; an abandoned transfer aborts on the idle deadline. Invalid foreign capabilities cannot cancel another Owner's transfer.
- `ota/finish`: exact byte count, SDK end/validation, persist CRC-protected receipt first, then select boot partition. Failure before selection retains current boot selection. No incomplete image is selected.
- Normal reboot through LockController. Pending-target boot blocks Access until original snapshot digest, actual running partition/image digest, database/Owner/config/canonical/HTTP initialization and saved STA reconnect pass. Offline verification fails after 120 seconds; it is not reported completed.
- `ota/result`: limited read-only receipt capability (hash only stored in NVS), with pending/completed/failed states and a bounded post-boot query window. It cannot authorize writes or unlock.

The running previous app partition is not erased by OTA. This does not add signed-firmware enforcement or a hardware watchdog rollback scheme for arbitrary future broken firmware. The controlled test must use the verified current image; serial flashing is not evidence of OTA partition switching.

Thai UI uses inline confirmation only, separate restore validation/activation, preparing/uploading/validating/rebooting/failure/completion messages, and retries transient network loss during reboot. It never calls native alert/confirm/prompt or rewrites Owner credentials.

## Security and regression evidence

| Suite | Result |
|---|---|
| Identity/storage/controllers | 355 assertions, 20 migration fault boundaries PASS |
| NetworkManager | 101 assertions, 8 scenarios PASS |
| RequestPolicy and sole lock-writer/unlock-authority audit | 43 assertions PASS |
| CloudProtocol (isolated; no Google deployment) | 38 assertions PASS |
| RestoreCoordinator production kernel | 192 assertions PASS |
| Concrete GenerationStore/AtomicFileStore/ConfigStore | 296 assertions; 14 NVS before/after commit cases; 26 SD fail-before mutation boundaries PASS |
| Production SnapshotValidator/AuthSnapshot | 20 semantic/malformed snapshot assertions PASS |
| Production OtaTransfer kernel | 113 checks PASS |
| Production OtaManager method bodies | 186 checks across 20 subprocess runs PASS |
| Touch/state, EventLog executable | PASS |
| Identity and maintenance browser VM | PASS |
| Embedded page JS syntax | All six scripts PASS |

Counted C++ host assertions/checks total **1,344**; uncounted browser/touch/event checks are not added to that number. Suite counts overlap scenario semantics and are not counts of independent physical tests.

GenerationStore tests compile the real backend and atomic file code but substitute SecureBackup capture/validation and in-memory SD/NVS. Snapshot tests separately compile the real semantic validators; on-board tests exercise real crypto. OtaManager tests preserve method bodies and substitute SDK/storage/auth/snapshot and a deterministic non-cryptographic hash. These host tests do not prove physical flash/card power-loss durability, real OTA transport or GPIO wiring.

Sol review caught and repaired original Setup-storage Owner credential reuse for OTA, recalibration of selected generations, invalid transaction generation IDs, corrupt-selector barrier handling, initialization gating, current-image digest verification, and transient reboot polling. GPIO22 remains written only by LockController; only AccessController calls unlock. Valid Owner Access is explicitly denied in both maintenance modes. Luna does not approve its own work.

## On-board evidence and final build

Final build and COM6 serial flash passed with write verification. This serial flash is not controlled OTA acceptance. Evidence is recorded under `evidence/phase13/`, including `build-manifest.json`, `final-build.txt`, `final-com6-flash.txt`, `final-boot-crypto-auth.txt`, `post-http-auth.txt`, `live-http-negative.txt` and `windows-health.json`.

Accepted checkpoint results:

- 14/14 RAM-only real crypto checks PASS; GPIO22 remained HIGH / LOCKED.
- 37/37 live HTTP rejection checks PASS. No destructive restore or real OTA upload was performed.
- Existing Owner verifier preserved; all four identities and all four verifiers intact, exactly one Owner. Owner identity, Wi-Fi configuration and touch calibration remain present.
- Normal boot reported configured state, healthy SD/TFT/touch, AP/canonical/HTTP initialization and automatic STA reconnection to 192.168.1.179.
- Minimum heap observed: **113,292 bytes**. No panic/watchdog reset observed in this verification.
- Windows numeric-IP health PASS (`/health` returned `ok`). Windows .local resolution remains DEFERRED. An initial HTTP connection timeout was followed by the successful 37-check run; it is recorded as a transient Windows/network observation, not proof of ESP32 connectivity loss.

Final binary SHA-256: `7daf74158a2f3d9cdad2d062d382da0dbbed345ce93150d8c3a41b79ab8ab322`.

Sol accepts the implementation/software and non-destructive on-board checkpoint. Full physical Phase 13 acceptance remains PENDING for restore activation and controlled real OTA upload. By the user's finalization instruction, no further suites, crypto diagnostic, restore, OTA, Phase 14, ADMIN PIN or Google Sheets work is started. This commit closes the current Phase 13 checkpoint only.

## Required physical checkpoint and Phase 14 carried items

Controlled OTA remains PENDING until the existing Owner browser downloads `smartlock-current.bin`, chooses that file in the Thai System OTA form, uploads it while locked, and observes completion after reboot. Then USB must confirm partition switch, GPIO22 locked, unchanged Owner/verifiers/config/STA/calibration and healthy heap. Final Owner Access/relock remains a user physical regression.

Real restore/power-cut testing that replaces live state is PENDING and must stop at an explicitly reviewed destructive checkpoint. No automatic restore fixture will use the production database.

Keep Phase 14 items: D000002 reboot revocation, Phone #3, paused Google, Windows hostname/HTTP reliability, changed DHCP, final Owner and Phone #2 regressions, explicitly authorized Factory Reset, Recovery AP, and physical backup/restore/OTA acceptance.
