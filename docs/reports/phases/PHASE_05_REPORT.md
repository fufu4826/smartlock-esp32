# Phase 5 — First Owner setup

Status: **PASS**.

## Design and ownership

- Sol Light owns the Setup session gate, input validation, first-owner transaction, credential and admin hashing, secure configured AP, recovery rules, and final PASS/FAIL.
- Luna High implemented the bounded mobile setup wizard. Sol Light reviewed it and removed persistent storage of the plain admin passphrase. The wizard retains pending random browser/AP credentials across a lost HTTP response and only stores the browser credential as active after success.
- The setup POST requires the current one-time physical Setup QR session. Browser-generated 256-bit credential and independent 128-bit AP password are sent once; the ESP stores a salted SHA-256 credential verifier and a PBKDF2-HMAC-SHA256 admin verifier (100,000 iterations). Neither plaintext credential nor passphrase is logged. The configured AP uses the generated WPA2 password after restart.
- The transaction writes AP secret, U000001 Owner, D000001 browser device, and the authentication record, validates their readback, and sets NVS `configured`/`ownerExists` flags last. A pre-commit interruption remains unconfigured and locked; valid interrupted `.tmp`/`.bak` setup files are repaired only while both NVS flags are false and then logically validated. A corrupt committed file is never silently repaired.
- Configured boot requires NVS flags, SD health, an active U000001 Owner and D000001 device, matching authentication entry, and the AP secret. Missing or corrupt data fails closed. GPIO22 logic is unchanged.

## Evidence so far

- Integrated PlatformIO build and COM6 upload: PASS.
- Serial boot: `SmartLock Phase 5`, `LockController: LOCKED`, `Configured: NO`, `Owner exists: NO`, `SD: OK (30000 MB)`, `SESSION TEST: PASS`, `AP: OK ... 192.168.4.1`, `DNS: OK`, `HTTP: OK`.
- PC AP preflight: DHCP PASS, setup wizard HTML loaded, missing POST rejected with 400, valid-shaped POST with a fake Setup token rejected with 403, and status stayed unconfigured. PC Wi-Fi was restored.
- Node wizard smoke test: generated credential/AP password have the required lengths, active and pending localStorage records behave as intended, clearing storage removes the credential, and the plain admin passphrase is absent from persisted browser records.
- User confirmed the valid Setup QR opened the wizard, real setup completed, the first browser/device registered, and a protected SmartLock AP was created. The earlier timeout came from the phone switching away from SmartLock Wi-Fi; no HTTP/WebServer fix was made for it.
- After reboot, Serial showed `Configured: YES`, `Owner exists: YES`, `SD: OK`, `LockController: LOCKED`, and `SCREEN: ACCESS QR` / `SCREEN: MANAGEMENT QR` transitions. The configured-data boot check requires the Owner, device, auth entry, and AP secret to be readable and consistent.
- PC scan saw `SmartLock-F0A4` advertising WPA2-Personal with CCMP. The PC's original Wi-Fi was restored afterward.
- Visual TFT Access/Management QR and energized magnet throughout: PASS. The user reconnected to protected `SmartLock-F0A4`, rebooted, saw ACCESS QR after one tap and MANAGEMENT QR after three separate taps, and confirmed the magnet stayed energized with no unintended unlock.

## Scope note

First setup uses the plan's open AP and local HTTP. A nearby observer could capture setup traffic. The physical QR token limits who can initiate setup but does not encrypt that traffic. This remains a security-review item for a production deployment.

## Decision

Sol Light independently reviewed the setup session gate, POST validation, write/readback order, boot integrity checks, NVS commit marker, configured AP password path, lock call sites, and Luna High's wizard. Phase 5 **PASSES**. Phase 6 may begin.
