# Focused physical Admin menu regression fix

## Physical defect and trace

User reported correct PIN returning immediately to Access. This supersedes the previous physical acceptance for menu retention: **physical retest pending**. No Factory Reset, PIN change, registration, identity change, Wi-Fi change, or Google work is authorized/performed here.

Traced production TouchManager -> main loop -> PhysicalAdmin -> AdminDisplay -> AppStateMachine. TouchManager emits SingleTap on debounced press, TapReleased on short release, and one Hold with release suppression. PhysicalAdmin verified the PIN on TapReleased and returned None on success; main's active-Admin branch did not directly fall through into AppStateMachine. Therefore the specific proposed same-event short-tap fallthrough is not established from source.

Two concrete weaknesses were found: inactivity timestamp was recorded before synchronous PIN KDF/NVS verification completed, and every Admin exit (including inactivity) was reset to Access. Admin accepted release events without requiring a new press. These paths are now isolated and tested. The exact physical exit trigger was not captured on the prior build; do not claim it was conclusively measured. Non-secret logs distinguish PIN-to-menu, Back, inactivity expiry and PIN-revision invalidation during the retest.

## Fix and safety

- Production AdminTouchRouter owns the full Admin gesture. Each button release requires a fresh press. Hold/release is consumed once; stale release cannot become Back, Emergency confirmation or a normal Access transition. Admin flow cannot fall through to the normal router.
- Successful PIN creates temporary authorization and Menu; inactivity timestamp includes verification elapsed time and begins at completion. Fresh loop time is used for Admin routing after potentially blocking web work. Expiry handles a stale earlier timestamp without unsigned-underflow expiry.
- Inactivity or revision invalidation clears authorization and returns IdleScreenOff. Explicit Back returns the safe Access screen. Menu remains active after confirmation release.
- Correct PIN/Menu never unlock. Router emits Unlock only for a new, explicit confirmation in the Emergency page. Main still applies its boot/storage/auth safety gate. LockController remains the sole GPIO22 writer.
- No changes to AdminPin verification/storage algorithm/value, Owner/auth storage, identities, Wi-Fi, browser credentials or calibration. UI strings/layout remain Thai and unchanged.

## Focused software acceptance

74 existing focused Admin/PIN/lock/config checks PASS with isolated NVS/crypto/GPIO fakes. New production router/state/touch checks cover all ten requested cases: correct PIN -> Menu; release retention; stale release rejection; wrong PIN -> Pin/error; explicit Back; inactivity authorization clearing; no unlock on Menu; Emergency requires fresh explicit confirmation; normal Access short tap; normal Management/second hold. Slow verification exceeding 30 seconds is simulated without real crypto work; Menu persists for its complete subsequent inactivity period.

GPIO source audit PASS. No Phase 13/14 large suite or crypto diagnostic executed. Build/deployment/preservation measurements are recorded below after completion. Physical menu retention remains pending until the user performs only Management -> second hold -> current private PIN -> release -> Menu remains visible.

## Production baseline reconciliation

The later user-confirmed post-cleanup physical PASS includes the current private PIN opening the Admin menu. COM6 flash verification now establishes that the Admin-menu fix image was already deployed before this reconciliation; no reflash was required. Earlier pending wording above is retained as historical investigation context, superseded for menu opening by that user evidence. Additional long-duration retention was not physically tested in this reconciliation.

`pio run` PASS with no source rebuild required; binary unchanged. 74 focused checks and all 10 router/gesture cases PASS; GPIO source policy PASS. Firmware SHA-256 `5b8df79ca74fd73edfd1ea4e2f238239607d322ef19b095614aa1e23a9eb0ae0`, size 1,241,520 bytes, app-slot headroom 69,200 bytes, static RAM 108,468 bytes.

Read-only esptool digest verification passed for firmware at 0x10000, partition table at 0x8000, and normal boot selection data at 0xe000. Thus the matching image is the boot-selected application, not merely an inactive slot. See evidence/admin_menu_fix/flash-verify.txt and build.json.

Normal boot/DIAG_AUTH and numeric-IP health PASS: GPIO22 LOCKED; configured Owner gugy and all four identities/verifiers intact; Owner verifier preserved; Admin PIN store OK; confirmed STA reconnect; calibration loaded; SD and audit healthy; no panic/watchdog. AdminPin implementation and NVS were not changed; no PIN attempt, extraction or reset was performed. Live checks establish store health, with current private PIN usability supported by the user's physical PASS.

This commit reconciles the formerly uncommitted PhysicalAdmin, AdminTouchRouter, main routing and bounded test changes with the deployed binary. No Factory Reset, browser storage clearing, registration, Google deployment or reflash occurred. Reset remains awaiting explicit authorization. Source baseline is the commit containing this report and build manifest; documentation-only commit metadata is not embedded in the binary.
