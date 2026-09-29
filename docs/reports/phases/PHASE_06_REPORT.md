# Phase 6 — Multi-device enrollment

Status: IMPLEMENTED BUT ACCEPTANCE PAUSED BY USER. The user intentionally reordered work to Phase 6A LAN/STA bootstrap before completing Phase 6 substitute validation. Do not treat Phase 6 as accepted or proceed to Phase 7 until the non-physical acceptance tests below pass.

## Implementation and ownership

Sol Light designed and implemented the session, authorization, role, revocation, credential uniqueness, database-integrity, and boot-safety paths. Luna High implemented the bounded management and enrollment web pages. Sol Light reviewed those pages and corrected the management QR session login and admin credential selection.

The management QR is a one-time 90-second physical-intent token. It requires an active Owner/Admin device credential before a separate five-minute management token is issued. Enrollment tokens are independent, single-use, and expire after two minutes. The enrollment URL carries no permanent credential. Phase 6 has no call to `LockController` and no unlock route.

## Evidence collected

- Build: `pio run -e esp32_035` PASS, RAM 35.1%, flash 71.0%.
- Flash to COM6: PASS, SHA verified, device rebooted.
- Serial: configured Owner retained; `LockController: LOCKED`, SD, DNS, HTTP, and session self-test PASS. The latest integrity/heap diagnostic firmware still needs flashing and inspection.
- Web page JavaScript syntax: `node scripts/phase6_asset_syntax.js` PASS for setup, manage, and enroll.
- `git diff --check`: PASS.

## Acceptance tests outstanding

- Use separate browser/storage contexts for Owner, Device #2, and Device #3; verify distinct credentials, Users #2/#3, independent enrollment, three Active devices, targeted revocation, consumed/expired session denial, privilege denial, duplicate credential denial, reboot persistence, and direct SD database integrity.
- Record API responses without copying any credential, password, or session token into this report.
- Confirm heap values and GPIO22 lock state on final firmware.

Physical enrollment using two additional independent phones was deferred by user request. Equivalent multi-device behavior must be validated using independent browser/storage contexts and direct database/API tests. Final physical multi-phone confirmation remains a deferred regression item in the Phase 14 checklist.

## User-requested ordering update (2026-09-26)
The later user instructions explicitly authorized Phase 6A, Phase 7, and Phase 8 while Phase 6 validation remains paused. The earlier do-not-proceed sentence is superseded by that authorization. Phase 6 is not marked accepted.
