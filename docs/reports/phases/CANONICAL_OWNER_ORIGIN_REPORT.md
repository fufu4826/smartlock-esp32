# Canonical origin and network-independent identity

Status: PASS for the clean canonical-origin physical Owner flow, per user acceptance. Changed-DHCP regression and Windows resolver reliability remain open regression items. This user-requested priority replaces normal AP-to-LAN bootstrap work. It is not acceptance of unrelated phases.

## Architecture owned by Sol

The authoritative microSD Users, Devices, roles, statuses, and salted credential verifiers are shared across every network interface. This was already true in the server; the earlier difficulty was browser origin separation, not separate authorization databases. No database format, Owner creation rule, AccessController, SessionManager, hashing, GPIO22, or LockController change is introduced.

The canonical origin is `http://smartlock-<full-48-bit-efuse-identity>.local`. It is computed from hardware identity, not an IP, SSID, SD record, or user. Full identity avoids relying on a short four-hex suffix. Factory reset and DHCP changes do not alter it. ESP-IDF mDNS advertises AP/STA interface addresses; the existing AP wildcard DNS also answers the canonical name at 192.168.4.1. HTTP service is advertised on port 80. This uses the pinned Arduino ESPmDNS wrapper over ESP-IDF mDNS, not Bluetooth.

Setup, Access, Management, and Enrollment QR URLs use the same hostname. Existing route shapes are preserved (`/setup?session=`, `/manage?session=`, `/a/<token>`, `/enroll?session=`). QR buffers remain bounded. Tokens are neither printed nor included in diagnostic logs.

Changing AP/STA changes reachability only. The same browser origin retains the opaque credential. The normal canonical flow has no bootstrap request, second registration, additional Device, or repeated Owner verification. Device creation remains limited to explicit Setup/enrollment/recovery operations.

## UI and recovery owned by Luna, reviewed by Sol

Thai landing/Setup links explain the canonical origin and preserve a fresh Setup QR query. Management shows the stable hostname and instructions to switch networks using the same browser. Its stable link goes to the root, avoiding reuse of a consumed Management session. Automatic bootstrap creation was removed. The legacy one-time bootstrap remains explicit recovery only on noncanonical origins, with a warning that it may add a Device. Numeric-IP pages and authenticated recovery routes remain available until Android/LAN replacement is physically verified.

Numeric-IP origins do not share browser storage with the canonical origin. Numeric access does not bypass authentication or automatically make a browser Owner. If name resolution fails, use numeric IP for diagnostics, report the client/network failure, and keep the existing authorized recovery process. No silent automatic credential migration is added.

Android captive-portal browser storage and regular Chrome storage can differ even for one hostname. Normal Setup and Access must use the same regular browser/profile. HTTP/localStorage retains the existing local-network threat model; mDNS does not provide TLS identity protection.

## Verified evidence (2026-09-26)

- Sol independently reviewed addressing changes and Luna's Thai UI. The stable Management link opens `/`, avoiding reuse of its consumed session. Only explicit recovery invokes bootstrap creation.
- Eight inline JavaScript programs pass syntax checks; firmware compile and COM6 upload PASS with image hash verified. RAM 124,576/327,680 bytes; flash 1,027,525/1,310,720 bytes.
- Serial after flash: LOCKED, lock timer OK, SD OK, session self-test PASS, database integrity PASS (2 Users, 1 Device, 1 ACTIVE, 0 REVOKED), canonical mDNS OK, saved STA reconnected to 192.168.1.179. Free heap 127,088 bytes / minimum 127,028 bytes at the reported boot checkpoint.
- The board now reports Configured YES and Owner exists YES. It changed since the previous clean-state snapshot. No agent Setup, credential seeding/restoration, configuration erasure, or factory reset was performed in this task. Existing records and saved network configuration were preserved. Original browser registration origin needs user confirmation.
- `scripts/canonical_lan_probe.py`: Windows OS resolves `smartlock-04225a0ff0a4.local` to 192.168.1.179; a direct multicast DNS A query returns that STA address. Canonical and numeric health/status/Thai Setup routes PASS.
- The existing wildcard DNS server, queried through the STA address on UDP53, answers the canonical name with AP address 192.168.4.1. This proves DNS responder mapping, **not** association/name resolution on an actual protected-AP client. No saved Windows profile for this protected AP is available; no password was retrieved or requested.
- `scripts/phase7_preflight.py smartlock-04225a0ff0a4.local`: invalid Access session, missing credential, incomplete Owner verification, unauthenticated bootstrap creation, and unknown bootstrap completion all denied. Read-only numeric recovery pages still respond. No valid unlock request was sent.
- Serial verifies logical LOCKED at boot; physical magnet behavior remains a human checkpoint. No failed requirement is marked PASS.

## Remaining acceptance limits

Actual protected-AP client resolution, Android resolution, same-browser localStorage continuity across the network switch, unchanged Device count during a valid Owner flow, and reboot after a genuinely changed DHCP address remain unverified. Reboot with the current saved STA configuration passed, but is not a changed-IP test. Full acceptance remains PENDING.

An Owner originally registered at a numeric IP cannot acquire that origin's localStorage merely by changing the firmware hostname. That existing browser requires the retained authorized recovery process or a deliberate fresh canonical-origin Setup. No automatic duplicate Device or Owner is created to hide this distinction.

Primary implementation reference: [Espressif ESP-IDF 4.4 mDNS documentation](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/protocols/mdns.html). Runtime client evidence above takes precedence over assumed client compatibility.

## Physical checkpoint

Use the same regular Chrome browser: connect SmartLock AP, open the canonical Setup QR, register once, configure home Wi-Fi, switch phone to home Wi-Fi, use the same hostname without any handoff link, single tap and scan a fresh Access QR, confirm immediate Owner recognition and timed unlock/relock. If canonical resolution fails on Android or LAN, stop and report it before registering through another origin or designing another fallback.

## Clean physical acceptance and final closeout (2026-09-26)

This section supersedes the earlier pending physical checkpoint and two-User snapshot. User intentionally factory-reset and completed canonical Setup in the same regular Chrome/profile on AP, then changed to home Wi-Fi. Registration occurred once; no handoff, second verification, or second registration. Fresh canonical Access QR immediately recognized the Owner, physically released the magnet, and automatically relocked. Consumed URL replay was denied with no second unlock. User physical result: PASS.

Sol repeated full build/COM6 flash (70.91 seconds, image hash verified), eight JavaScript syntax checks, UTF-8 metadata checks, source/diff review, and boot Serial. Current post-reset SD database: users=1 devices=1 active=1 revoked=0, integrity PASS. Boot validation requires ACTIVE U000001 Owner and ACTIVE D000001 associated with U000001, with a valid verifier record. Together with the user's successful authorization, this confirms the expected single Owner/device; no LAN-origin duplicate. Verifier/credential material was not printed. Saved STA reconnected to 192.168.1.179; mDNS initialized OK; independent direct multicast query returns that address. GPIO22 starts LOCKED; relock timer OK; SD/session tests PASS; heap free127088/min127028. Numeric recovery routes and invalid API rejection PASS after reboot.

Windows OS hostname lookup passed before this reboot but failed during final closeout, including after DNS cache flush. Direct mDNS still returned the correct STA address, and numeric LAN health/API checks passed. This is recorded as a Windows resolver reliability regression, not hidden by a hosts-file override or claimed universal name-resolution PASS. Actual Android Chrome AP-to-LAN flow is physically PASS. No new fallback architecture is introduced.

Obsolete popup/postMessage handoff routes remain unregistered (disabled). Normal canonical UI has no automatic bootstrap call; authenticated explicit numeric-origin recovery remains available. AccessController/LockController/authentication requirements remain intact. Physical Phones #2/#3 enrollment/revoke remains DEFERRED. Changed DHCP address is not tested and remains Phase14 regression work.
