# Production scope simplification — Backup/Restore and browser OTA removal

## Decision and status

User-requested scope change after the accepted Admin PIN physical checkpoint. Implementation, focused software tests, full build, COM6 flash, and live preservation checks: **PASS**. Previous Admin physical acceptance remains PASS. Post-cleanup authenticated physical Access/PIN regression is user-confirmed **PASS**; from-zero Factory Reset remains prepared only. Google remains paused.

## Removed production functionality

Removed encrypted backup export/package crypto, restore validation/staging/activation/generation transactions, OTA manager/transfer/session/upload/current-image download, their HTTP handlers/routes, UI controls/status panels, parser exceptions, and maintenance-only diagnostic. Historical Phase 13 reports/evidence remain unchanged. Obsolete tests are archived under `tests/archive/phase13`, explicitly excluded from current acceptance.

Retained atomic storage/CRC validation, credential verifier protection, identities, roles, one-use sessions, Access, LockController, Recovery AP/STA, canonical hostname, local events/audit, Admin PIN/emergency unlock/Factory Reset. Internal identity migration backups remain necessary for verifier preservation; these are not a user Backup/Restore service. The partition table is unchanged. Future firmware updates use USB COM6.

## Storage and security review

Read-only preflight confirmed this board uses the original `legacy_current` store with four identities/verifiers. No data migration or erasure was required. Normal config A/B records and original identity/auth paths are retained. A minimal guard rejects a retired restore selector/journal rather than activating stale original credentials on an unsupported restored installation. It neither activates nor erases a generation. Future explicitly authorized Factory Reset retains cleanup of retired namespaces.

Owner-only PIN authorization, lock/network-transition guards, role enforcement, and Access session/ACTIVE identity/verifier checks remain. Source GPIO policy passed: only approved Access and confirmed physical Admin paths call unlock; LockController retains GPIO ownership. PIN implementation is unchanged, no PIN was read, guessed, logged, changed, or reset. NVS and SD operational state were preserved by the flash.

## Focused verification

- 74 C++ Admin/PIN/LockController/config checks PASS with isolated mocks, including config persistence, corruption rejection, and unsupported retired-store rejection.
- Admin web script/PIN controls tests PASS; enrollment browser identity tests PASS; cleanup Management DOM/control/routes/parser tests PASS.
- Actual Access/Enrollment/Identity/AtomicFile/Session production code smoke PASS with isolated storage/auth/GPIO fakes: migration/verifier preservation, authorized Access, replay and wrong-credential rejection, Owner Management without unlock, storage fault denial.
- Full production build PASS; COM6 upload PASS with flash hash verified. No large historical suite or crypto diagnostic rerun.
- Live normal boot PASS: Configured YES, Owner YES, one Owner/four identities/four verifiers, Owner verifier preserved; gugy D000001 OWNER ACTIVE and boom/Root/Ki remain USER ACTIVE. No changes to their state were requested or made.
- Admin PIN store OK, touch calibration loaded, SD OK, audit READY, GPIO22 LOCKED, STA auto-connected at 192.168.1.179; canonical mDNS initialized. No panic/watchdog observed. Minimum heap in this capture: 121,800 bytes.
- Numeric-IP health PASS, Management page contains PIN form and no removed controls. Nine removed routes tested with both GET and POST: 18/18 returned 404. Unauthorized PIN route returned 403 without PIN verification. No live unlock/reset/restore/OTA action executed.

Evidence: `evidence/product_cleanup/before-boot.txt`, `after-boot.txt`, `http.json`, `build.json`. Existing optional cloud-slot NOT_FOUND messages occur in both pre/post boots; cloud is disabled and they did not prevent healthy boot.

## Size

| Metric | Before | After |
|---|---:|---:|
| Firmware binary | 1,288,880 bytes | 1,241,072 bytes |
| Existing app slot | 1,310,720 bytes | 1,310,720 bytes |
| Binary headroom | 21,840 bytes | 69,648 bytes |
| Static RAM | 109,148 bytes | 108,468 bytes |

Recovered flash: **47,808 bytes**. Static RAM reduced by **680 bytes**. Linked program size: 1,234,497 bytes (94.2%). Binary SHA-256: `f3b5868d34edbf2086702a563cfcd045cf6e59ca634f75a04b24e952c09b4e46`.

## Physical acceptance and remaining checkpoint

User confirmed the cleanup checkpoint physically accepted:

- Existing Owner fresh ACCESS QR unlock: PASS.
- Timed automatic relock: PASS.
- Management System has no Backup/Restore/OTA controls: PASS.
- ADMIN PIN controls remain present: PASS.
- Current private ADMIN PIN opens the physical Admin menu: PASS.

This records the user's evidence for cleanup checkpoint `65ee81de8f71ca3df5a56de0ce91289fa5a3120e`; no new physical test was performed by Codex. Separate uncommitted Admin-menu work is not approved or committed by this documentation update.

From-zero Factory Reset is still preparation only, requiring explicit execution instruction. Windows `.local` resolution and Phone #3 remain deferred. Google deployment remains paused. Historical restore/OTA physical acceptance is superseded by feature removal, not retroactively marked PASS.
