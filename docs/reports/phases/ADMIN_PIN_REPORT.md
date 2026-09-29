# Physical Admin PIN feature

Phase 14 was accepted for continued development with remaining physical checks USER WAIVED / NOT TESTED (documentation commit 51c1086). No physical restore, controlled OTA or D000002 reboot revocation PASS was manufactured.

## Implementation and Sol security review

- Exactly four numeric digits. Existing configured installation bootstraps the development PIN only if its verifier is absent. Existing verifier is never overwritten during reflash. New first-time Setup requires the Owner's chosen PIN and confirmation; PIN is excluded from persistent browser storage.
- `sl-pin` stores a random 16-byte salt and PBKDF2-HMAC-SHA256 verifier (10,000 iterations), never plaintext. Constant-time verifier comparison; transient buffers cleared. No new library. Namespace is independent of legacy authorization/Wi-Fi and restore generations: existing encrypted backup format does not export or roll back this new PIN. Factory Reset clears the PIN namespace and preserves existing touch-calibration policy.
- Three failed completed attempts lock entry for 60 seconds. Failure count persists per completed attempt; successful verification resets it. Reboot restarts a full 60-second wait when the persisted count is at least three; it does not bypass lockout. No countdown writes.
- Second hold on Management enters the red Thai PIN page and locks through LockController. Four masked slots, large keypad, delete, cancel. Temporary physical authorization expires after 30 seconds inactivity and is invalidated by PIN change. It cannot issue browser credentials, enroll identities or alter Wi-Fi.
- Menu entry does not unlock. Emergency unlock requires a separate affirmative tap and uses configured duration through LockController, with storage/initialization/maintenance checks. Audit action `EMERGENCY_UNLOCK_ADMIN_PIN` records success/denial without PIN. LockController remains the sole GPIO22 writer; this explicitly authorized physical path joins the existing AccessController unlock path.
- Factory Reset requires PIN, first confirmation, then a separate final hold. It locks and verifies GPIO22 HIGH before erase. No real reset was executed in development.
- Owner-only authenticated POST `/api/system/admin-pin`: current PIN, new PIN and matching confirmation; bounded form, maintenance/lock safeguards and existing Owner authorization. No PIN in URL, localStorage or logs. Browser Owner credential, identities and Wi-Fi are not rewritten.

## Focused tests

65 checks PASS compiling production AdminPin, PhysicalAdmin and LockController with isolated NVS/crypto/timer/GPIO fakes: bootstrap, chosen Setup PIN, persistence/reinitialization, malformed digits, wrong/old/new PIN, third-failure lockout and reboot retention/expiry, delete/cancel, menu/confirmation, final reset hold, authorization timeout/invalidation, timed relock and storage failure. These are not physical wiring or real reset evidence; crypto fake is explicitly non-cryptographic.

Focused web test PASS: successful/rejected PIN-change UI, mismatch rejection, clearing fields, Setup PIN excluded from persistent storage, Owner-only backend guard review. Changed Setup/Management script syntax PASS. Thai bitmaps: 16 labels, maximum width 240 pixels. Source GPIO writer audit PASS. No unrelated large Phase 13/14 suite or crypto diagnostic rerun.

Initial full build passed. A focused final rebuild corrected a missing AuthStore include added by the storage safety check; final build PASS. No partition change.

## Capacity

Before: firmware 1,276,288 bytes; slot 1,310,720; headroom 34,432.

After: firmware **1,288,880 bytes**; slot **1,310,720**; headroom **21,840**. Program 1,282,297 bytes (97.8%); static RAM 109,148 bytes (33.3%). SHA-256 `2754c0a51a4dbb1fd912cc673787fa1fffe261673048aef10f37594d4428274d`. Generated partition table matches the read-only production dump.

## Live board and physical status

COM6 flash PASS with hash verification. Normal reboot PASS: GPIO22 LOCKED, timer ready, configured/Owner flags intact, SD healthy, calibration loaded, PIN store OK, canonical/mDNS/HTTP initialized and saved STA automatically reconnected to 192.168.1.179. Read-only DIAG_AUTH: gugy D000001 OWNER ACTIVE and original verifier preserved; four identities/four verifiers, Root/Ki/boom unchanged. Minimum heap 119,436 bytes in this boot; no panic/watchdog observed. Evidence: `evidence/admin_pin/boot-auth.txt`.

New served Management page present; numeric `/health` PASS; unauthenticated PIN change denied (403), malformed request rejected (400). No successful PIN change, unlock or reset was issued automatically. The HTTP fixture initially expected `ok` without the endpoint's newline; fixed test comparison only, no firmware change. Evidence: `evidence/admin_pin/http.json`.

Implementation/software checkpoint PASS. Physical Admin acceptance **PASS**, explicitly confirmed by the user:

- Management second hold opens the red PIN screen; bootstrap PIN accepted and Admin menu opens.
- Emergency unlock confirmation, physical magnet release and timed automatic relock PASS.
- Owner web PIN change PASS; new PIN accepted on TFT; previous bootstrap PIN rejected.

The changed PIN was not requested, collected or logged. This acceptance does not claim physical lockout, Factory Reset, new from-zero Setup, restore or OTA acceptance. Factory Reset and from-zero Setup remain untested. User authorized preparation only of the final from-zero acceptance phase; no reset is permitted yet. Google remains paused.
