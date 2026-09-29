# Final from-zero Factory Reset acceptance — prepared, not started

## Authority and baseline

Physical Admin acceptance is user-confirmed PASS at source checkpoint c3c9dace2061bd19736e4f627fed94138d179224. Subsequent product cleanup removed Backup/Restore and browser OTA; see PRODUCT_CLEANUP_REPORT.md. Cleanup checkpoint firmware (reconfirm deployed build before execution): 1,241,072 bytes; unchanged app slot 1,310,720; binary headroom 69,648. SHA-256: f3b5868d34edbf2086702a563cfcd045cf6e59ca634f75a04b24e952c09b4e46.

Current instruction authorizes **preparation only**. Do not Factory Reset, erase credentials, clear browser storage, re-register Owner, reconfigure Wi-Fi, reflash, or start Google Sheets now. No new suites required for preparation. Execution requires a separate explicit user instruction to begin the real reset cycle.

## Prepared execution gate

Cleanup checkpoint `65ee81de8f71ca3df5a56de0ce91289fa5a3120e` has user-confirmed physical PASS for Owner unlock/relock, removed maintenance controls, retained PIN controls, and current PIN opening Admin. This is a documentation-only preparation checkpoint: no reset, flash, reboot, credential change or Google action is performed.

Admin-menu source/tests have now been reconciled with the already deployed COM6 image and committed with ADMIN_MENU_FIX_REPORT.md. Current production baseline: firmware 1,241,520 bytes, SHA-256 `5b8df79ca74fd73edfd1ea4e2f238239607d322ef19b095614aa1e23a9eb0ae0`; unchanged slot with 69,200 bytes headroom. Read-only verification matched the app, partition table and boot selection data. Normal boot preservation passed. Use the reconciliation commit containing that report as the execution baseline; the earlier cleanup size above is historical. No reset authorization has been given.

**WAITING FOR EXPLICIT USER AUTHORIZATION TO EXECUTE FACTORY RESET.** All reset/fresh-setup/regression steps below remain NOT STARTED. A message accepting cleanup or requesting preparation is not reset authorization.

Once authorized, proceed through A, B and C below. Keep the current PIN private; no credential or Wi-Fi password needs to be sent to Codex. Report each observed result as PASS, FAIL or NOT TESTED. Stop at a real failure and repair the affected path before continuing. Google stays paused even after this checkpoint.

## Execution checkpoint A — before the first reset

When separately authorized:

1. Confirm known-good commit/tag and firmware hash; inspect current board metadata read-only. Record current Owner/identity counts, configured state, SD, calibration and canonical hostname without exposing verifiers or credentials.
2. Record non-secret configuration choices needed for fresh Setup and retain the current PIN privately for entry before reset. Backup/Restore and browser OTA have been removed by subsequent product simplification; do not look for those controls or require an encrypted backup.
3. Confirm GPIO22 LOCKED and tell the user the concrete scope: this reset deletes identities/verifiers/Owner registration, home Wi-Fi/AP secrets, operational configuration, ADMIN PIN, cloud state, restore generations and OTA receipts. Touch calibration is preserved. Existing browser credentials will cease authorizing the board.
4. User enters the current private PIN, opens Factory Reset, checks Cancel on the first confirmation, re-enters, proceeds to final confirmation, checks that a short tap does not reset, then deliberately holds the final reset button. Do not remotely bypass PIN or confirmations.

## Execution checkpoint B — reset and fresh Setup

- Observe normal reboot with GPIO22 LOCKED, Configured NO, Owner NO, no authorized identities/verifiers, no confirmed saved STA and no ADMIN PIN verifier. SD remains available; touch calibration remains loaded. Capture reset/boot metadata and any failures. Do not treat intentional empty state as a persistence defect.
- Confirm blue SETUP screen and white QR background. Connect to the setup AP and use the canonical hostname in the intended normal browser.
- Complete fresh Setup: chosen Owner name, new browser credential, chosen unlock duration and an explicitly chosen four-digit ADMIN PIN with matching confirmation. Choose a PIN different from the old one and the development default; never send it to Codex.
- Confirm exactly one ACTIVE OWNER D000001 and one credential verifier; no old identities or duplicate Owner. Configure home Wi-Fi through Management, follow the canonical-origin transition and verify automatic Owner recognition without re-registration.
- Old browser credentials, if still available in an independent browser context, must be denied on a fresh Access QR. If overwritten during Setup and unavailable, record that browser check as NOT TESTED; verify old database records were removed rather than inventing a PASS.

## Execution checkpoint C — combined functional regression

1. Owner fresh ACCESS QR: real unlock and configured timed relock. Used URL/session replay denied.
2. Management hold: yellow screen, white QR, Owner login and Thai sections; Management causes no unlock.
3. Add one fresh non-Owner identity using an available second phone/browser. Enrollment QR has white background; independent credential, one-time enrollment and fresh Access/unlock/relock work. Revoke only that identity; fresh Access denied, magnet LOCKED; Owner still unlocks/relocks.
4. New chosen PIN opens red Admin menu. Emergency unlock requires separate confirmation and relocks automatically. Old PIN/default rejected. Delete, Cancel and inactivity expiry clear local authorization; no automatic unlock. Exercise three wrong attempts and the 60-second lockout using the known chosen PIN afterward; record actual results.
5. Owner web PIN change: current PIN required, matching four-digit replacement, replacement works locally, old PIN rejected. Do not share any PIN.
6. Normal reboot: exactly one Owner, new credential remains valid, revoked identity remains REVOKED/denied, changed PIN remains valid, Wi-Fi reconnects, canonical hostname returns, calibration/SD/database remain healthy, boot LOCKED with no panic/watchdog. Owner fresh Access still unlocks/relocks.

Use the fewest grouped physical checkpoints. After each reboot, Codex collects authoritative non-secret metadata. Fix only demonstrated defects and rerun only affected checks. No partition changes or quota-heavy unrelated suites.

## Optional second reset — explicit decision required

The first real reset plus confirmation/cancel tests normally establishes the reset flow. Do not automatically perform a second reset that would erase the newly accepted setup. If a second full reset is needed to resolve a demonstrated defect, explain why and obtain explicit authorization before erasing; repeat necessary Setup/regressions afterward.

## Acceptance and stop

Create a concise final report separating PHYSICAL PASS, LIVE BOARD PASS, NOT TESTED, DEFERRED and defects. This plan is not test evidence. From-zero acceptance requires actual reset, fresh chosen-PIN Setup, canonical Owner recognition, Owner/second-device Access, revoke/reboot persistence, emergency timed unlock, Owner PIN change and safe reboot preservation.

Historical Phase 14 restore/OTA checks were USER WAIVED / NOT TESTED. Those features are now intentionally removed and are not final-product acceptance requirements. Windows .local, Phone #3 and naturally unobserved changed DHCP remain deferred. Google Sheets remains paused throughout this phase and does not start automatically at completion. After final acceptance, commit the verified checkpoint and stop for the next user instruction.
