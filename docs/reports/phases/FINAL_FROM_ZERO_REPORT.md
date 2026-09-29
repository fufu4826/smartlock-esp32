# Final from-zero acceptance - IN PROGRESS

## Authority and baseline

User explicitly authorized one destructive reset cycle in attachment 728dd0d9-e7b5-4120-bf29-4ccff48a6314. Physical PIN and confirmations must not be bypassed. No second reset authorized. Google remains paused.

Starting commit: 6ca306da3f753bdf5cf58a41a319637748447521. Working tree clean before this report/evidence. Firmware 1,241,520 bytes; SHA-256 5b8df79ca74fd73edfd1ea4e2f238239607d322ef19b095614aa1e23a9eb0ae0. Local hash reconfirmed; deployed digest/boot selection verified at preceding reconciliation checkpoint. No firmware changes made.

## LIVE BOARD PASS - checkpoint A

Normal reboot and read-only DIAG_AUTH captured in evidence/final_from_zero/pre-reset-boot.txt. GPIO22 LOCKED; Configured YES; Owner YES; one Owner, four identities, four verifiers; Owner verifier preserved; SD OK; touch calibration loaded; canonical smartlock-04225a0ff0a4.local initialized; saved STA reconnected to 192.168.1.179; Admin PIN store OK; audit ready; numeric health OK. No PIN entry, credential extraction, unlock or reset performed by Codex.

## PHYSICAL results

Reset Cancel, final short-tap rejection, deliberate hold/destruction, reboot, blue SETUP and magnet remaining LOCKED: user-confirmed PHYSICAL PASS. Exactly one destructive reset performed by user. Subsequent one normal reboot captured in evidence/final_from_zero/post-reset-normal-boot.txt.

Fresh Setup/PIN, Wi-Fi transition, Access/replay, second identity enrollment/access/revoke, Admin controls/lockout/emergency/PIN change, reboot persistence and final GPIO22 state: NOT TESTED in this cycle.

## Deferred and non-requirements

Windows .local, Phone #3, changed DHCP unless naturally observed. Backup/Restore/OTA removed. Google paused.

## Defects

Fresh Setup UI defect reported before submission: separate device name and obsolete Admin passphrase. Acceptance paused for focused cleanup; see SETUP_SIMPLIFICATION_REPORT.md. This is not a final PASS report.

## Post-reset functional factory state - ACCEPTED by user

Boot evidence: LockController/GPIO22 LOCKED, Configured NO, Owner NO, PIN verifier NOT_FOUND (no default initialized), SD OK, calibration loaded, setup AP SmartLock-F0A4 at 192.168.4.1, canonical hostname/mDNS healthy, HTTP/DNS OK, cloud disabled/unconfigured. No STA reconnect or panic/watchdog/unlock observed in the capture. TFT blue SETUP confirmed physically; captured boot subsequently reports screen off.

Storage-forensics limitations: unconfigured boot does not independently inspect saved STA slots or enumerate absent identity/auth files. DIAG_AUTH valid=0 with zero counters short-circuits for missing initialized data and is not independent proof of erasure or evidence of corruption. User accepted these as limitations, not functional reset failures. No firmware/diagnostic additions authorized or made.

Fresh production Setup and home-Wi-Fi configuration are now authorized for the user. Awaiting completion and non-secret LAN address/recognition results. Old independent browser credential check remains NOT TESTED until availability and actual denial are established. No credentials/PIN/password requested or recorded. Google remains paused.

## Setup defect fixed; acceptance resumes

Focused simplification built/flashed; fresh state preserved. See SETUP_SIMPLIFICATION_REPORT.md and evidence/setup_simplification. Firmware 1,239,632 bytes, SHA-256 fbf3def2aaa039678a5ef9c4f9e55e728067224ebf93244009037f4884799b09. No fresh Setup submission yet. Next physical checkpoint: four-field Setup, chosen private PIN, then intended home-Wi-Fi Management flow.

## Stale browser enrollment defect

After user-completed fresh Setup, current Owner ko/D000001 and one verifier were observed. User reported old Phone #2 blocked by local-only credential presence. Focused fix deployed; see STALE_ENROLLMENT_REPORT.md. Owner/PIN/Wi-Fi/calibration preserved. One physical stale-browser enrollment/Access retest pending; no second reset or Google work.

## Current acceptance progress after stale-browser physical PASS

User-confirmed PHYSICAL PASS: Phone #2 retained pre-reset browser storage, accepted a fresh invitation, registered hi, appeared in Management, then fresh Access physically unlocked and automatically relocked. This closes the stale-browser defect. Read-only DIAG_AUTH now confirms ko/D000001 OWNER ACTIVE, hi/D000002 USER ACTIVE, two identities/two verifiers/one Owner; database valid. Evidence: after-phone2.txt. No reboot or authority mutation performed during this check.

Remaining physical acceptance: Owner Access/replay; current Owner canonical-origin recognition and Thai Management without unlock; used enrollment replay denial; revoke only hi and verify denial/Owner continuity; chosen PIN/old PIN/default rejection, Delete/Cancel/timeout, three-failure 60-second lockout, confirmed emergency unlock/relock and audit; Owner web PIN change; final normal-reboot persistence including revoked hi and changed PIN. New PIN and passwords remain private. Old pre-reset credential door-access rejection is NOT TESTED unless another independent old context exists; successful stale credential replacement is a separate accepted behavior.

Awaiting grouped physical results before the final normal reboot. No full acceptance PASS/tag yet. Google remains paused; no second Factory Reset.
