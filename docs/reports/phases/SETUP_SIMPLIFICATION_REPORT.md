# Focused final Setup simplification

## Defect and scope

User stopped fresh Setup before submission because the form retained device-name and Admin-passphrase concepts. Board was accepted fresh/unconfigured with absent PIN. Only this Setup/auth cleanup is authorized; no second Factory Reset, real Owner creation, Wi-Fi configuration or Google work.

## Authorization review

All production references were traced. Legacy Admin passphrase had no current authorization caller; verifyAdmin had no caller and the remaining passphrase use was Setup creation/readback. Owner browser credentials authorize Management; physical PIN authorizes local Admin. Removed passphrase UI, request/input validation, hash function, verification API, salt/hash/iteration fields, and crypto dependencies used solely by it. Browser credentials remain independently random 256-bit secrets, with salted SHA-256 verifiers and constant-time comparison on the board. Physical PIN does not authorize browser Management.

## Final UI and transaction

Thai fields in order: Owner name, unlock seconds (1-60), four-digit ADMIN PIN, PIN confirmation. Submit label: ตั้งค่าอุปกรณ์. Separate device/browser name is absent from form, request, FirstOwnerInput and Setup validation. IdentityStore creates D000001 using the chosen normalized name, OWNER ACTIVE, with one browser credential. The fixed internal hardware/config label SmartLock remains in the config layout; it is not a user input or identity naming requirement.

PIN and matching confirmation are enforced on server and browser; 1234 rejected for fresh Setup. Neither PIN field persists in pending browser storage; fields clear after submission. Browser credential and AP secret are independently generated automatically. No automatic default PIN is created even for an inconsistent configured-but-missing-PIN boot (that condition fails closed).

AuthStore schema 2 contains only magic/schema/count plus credential entries. Schema 1 is rejected, not migrated or reinterpreted. This fix is scoped to the already reset board with no submitted Owner; it is not an upgrade/migration for configured schema-1 installations. Existing atomic integrity checks, one-time Setup session, last-written configured commit marker, role checks, GPIO gate and lock timer remain. First-owner storage refuses to replace any existing authority.

## Focused verification

- Production Setup script executed in isolated DOM/storage contexts: exact four inputs, seven protocol fields including generated session/credential/AP secret, mismatched/invalid/default PIN rejection, independent credentials, active credential persistence and PIN clearing PASS.
- Production FirstOwnerSetup/AuthStore method bodies compiled with isolated dependencies: invalid/missing/default PIN, mismatch, chosen name, configured transaction, one D000001 verifier, no replacement, schema 2 layout without passphrase fields, duplicate credential rejection, second credential independence, old schema rejection PASS. Crypto stand-in validates control/storage behavior only, not cryptographic strength.
- 78 Admin/PIN/config/lock checks and 10 router/gesture cases PASS. Admin web checks PASS. Focused core Access/Management/storage smoke PASS. GPIO source policy PASS; Setup has no unlock path.
- Full build PASS. No broad historical suites or live Owner creation.

Firmware: 1,239,632 bytes; static RAM 108,468 bytes; app slot unchanged at 1,310,720 bytes; headroom 71,088 bytes. SHA-256 fbf3def2aaa039678a5ef9c4f9e55e728067224ebf93244009037f4884799b09. Evidence in evidence/setup_simplification.

## Deployment

COM6 upload PASS, flash hash verified; normal boot PASS: GPIO22/LockController LOCKED, Configured NO, Owner NO, PIN verifier NOT_FOUND, Setup AP SmartLock-F0A4 at 192.168.4.1, SD OK, calibration loaded, canonical mDNS and HTTP OK. No STA connect, panic, watchdog or unlock observed. No Owner or Wi-Fi configuration submitted; no Factory Reset performed. Fresh physical Setup remains pending; resume using a fresh QR and reload the page after deployment. Google remains paused.
