# Thai web localization before Setup

Scope: user-requested presentation-only localization before Owner registration. User communications remain English. The factory-reset Configured NO / Owner exists NO state is intentional and must remain unchanged until the user completes Setup.

## Ownership and boundaries

Luna High translates embedded HTML/JavaScript display copy. Sol reviews completeness, UTF-8, responsive rendering, protocol preservation, and security-sensitive handlers. Existing Setup, Access, Management, enrollment, network, bootstrap, and recovery logic is retained. No Phase 8 dashboard features or new APIs are introduced in this pass.

The captive portal redirects to the root page; it has no separate HTML asset. Current pages are Index, Setup, Manage (users/devices and network), OwnerBootstrap, Enroll, Access, VerifyOwner recovery, plus unused Network/Reset assets. Dedicated Access Settings/System screens do not exist yet; their Phase 8 implementation is deferred. Success/failure/loading messages are embedded within these assets.

## Audit classification

Intentional English: SmartLock, Wi-Fi, SSID, IP, LAN, AP, STA, RSSI, microSD, ID, HTTP, UTF-8, ASCII, dBm; routes, JSON keys/error codes, enum values, CSS/DOM identifiers, storage keys, and internal default device names. User data and network SSIDs are preserved. Role/state values are translated when rendered in human-facing controls/cards.

Visible plain-text web responses are Thai and explicitly UTF-8. The health response `ok` and diagnostic JSON protocol values are unchanged. Authentication checks, HTTP statuses, route registration, and response JSON schemas are unchanged.

## Verification

- Full source audit reviewed all nine assets/fragments. All eight HTML documents specify Thai language and UTF-8. Remaining English display candidates are intentional technical terms; normal English UI copy has been translated. A missed handoff paragraph and Management required-field validation were caught and repaired during Sol review.
- All seven embedded JavaScript programs pass syntax checks. Required-field errors use Thai custom validity text while existing constraints remain unchanged. Roles/status/STA state are mapped only at display time.
- API path and browser storage-key sequences match the pre-localization assets. Hashes of ten critical implementation files (LockController, AccessController, SessionManager, FirstOwnerSetup, EnrollmentManager, NetworkManager, FactoryResetController, AuthStore, ConfigStore, StaSecrets) are unchanged. Normalized script review found only validation copy support and display mappings beyond string replacements. Server response changes affect visible plain text and charset only.
- Full firmware build PASS: RAM 122,584/327,680 bytes (37.4%), flash 996,333/1,310,720 bytes (76.0%). COM6 flash PASS with image hash verification. No NVS erase, factory reset, Setup POST, Wi-Fi configuration, Owner seeding, or authorized unlock was performed.
- Windows in-app Chromium preview inspected Index, Setup with/without a synthetic session, Management/Network layout, Enrollment layout, OwnerBootstrap error, Access error, VerifyOwner recovery, and unused Reset layout. Thai glyphs/tonemarks were readable at 360-pixel mobile width; Setup and Management had no horizontal overflow. Initial Setup was also checked at 1280-pixel desktop width. Empty required Setup field visibly produced Thai validation. Layout fixtures disable scripts and expose hidden sections; no authenticated dashboard operation is claimed.
- The PC remains on home Wi-Fi. Direct HTTP verification of the ESP32 AP page is not available from that network; live AP/Android visual confirmation is the user checkpoint. Source route registration and local embedded-page response were verified.

Final Serial boot/heap results are recorded below.

The read-only localhost preview uses the embedded assets, rejects every POST, and never contacts the ESP32. Its layout mode exposes hidden sections with scripts removed, solely for layout inspection. Preview rendering is not configured-device acceptance or Android hardware proof.

## User checkpoint

After successful software verification, the user should connect to SmartLock-F0A4 and visually inspect the initial root/Setup UI. Do not complete Setup until the user confirms the Thai UI. Android Chrome and live AP-page visual confirmation remain the user checkpoint. Phase 7 persistence recheck/commit and Phase 8 resume after this checkpoint and the subsequent user-performed Setup.

## Final runtime result

COM6 Serial observed for 32 seconds after reset: one application BOOT, LOCKED, lock timer OK, Configured NO, Owner exists NO, TFT/touch OK, SD OK, session self-test PASS, SmartLock-F0A4 AP at 192.168.4.1, DNS/HTTP OK, READY. No application crash/reset loop was observed. Free heap 139,052 bytes; minimum free heap 138,764 bytes. The clean factory state is preserved.

Status: software localization/build/flash review PASS; live phone visual acceptance PENDING. No Phase 8 acceptance or configured persistence acceptance is claimed. Next action is visual inspection only, with Setup deferred until user approval.
