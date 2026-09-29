# LAN-primary policy and Phone #2 enrollment checkpoint

User-requested architecture/UX reordering, 2026-09-26. This work extends the existing Phase 6–12 implementation. Real Google deployment is paused, not abandoned or marked PASS. No production OAuth/backend is being built. The next human checkpoint is one real Phone #2 enrollment/access/revoke test; Phone #3 remains deferred.

## Policy and authorization review

- The existing canonical hostname and Owner browser credential remain unchanged. No AP-to-LAN credential handoff is required on the canonical origin.
- Initial AP is for the first Owner setup. Additional User creation and all enrollment creation/check/redemption require a request received on the connected STA interface. The server checks the socket's local address, not a client-supplied Host, role or network flag.
- The protected AP remains available during a candidate STA transaction. A real LAN API/health request verifies reachability; after three seconds for the response, the radio becomes STA-only. Scan does not silently re-enable AP. A sustained STA failure restores the protected recovery AP after sixty seconds; saved-STA retries and BOOT recovery remain available.
- Before first LAN verification and during recovery, AP Management and emergency Access accept only an existing active Owner device. No additional enrollment or Add User is allowed over AP, even for Owner.
- Owner may create Admin/User/Guest. Admin may create User/Guest only, and may not enroll/revoke/disable Owner or Admin. A second Owner cannot be created. User/Guest cannot enter Management.
- Owner, Admin, User and Guest have ordinary door permission when both their User and Device are ACTIVE and the credential/session validate. Non-Owner access additionally requires home LAN. Guest currently means ordinary door permission until revoked/disabled; no automatic Guest expiry is implied.
- AccessController remains the only authorization path to unlock; LockController alone owns GPIO22. Session consumption precedes unlock and timed relock is unchanged. Successful events use the actual authorized User ID, rather than a fixed Owner ID.
- Enrollment tokens retain 256-bit device-generated randomness, a separate SessionType, a 120-second lifetime and one-use consumption. The issuer Device and target User are revalidated at redemption. An inactive/revoked issuer or target denies enrollment. Tokens never contain permanent credentials.
- Phone #2 generates its own opaque credential using browser crypto.getRandomValues and stores it only in its own browser. Existing AuthStore salted verifiers and duplicate-credential rejection are retained. No Owner credential/passphrase is copied or exposed.
- Phone #2 supplies its own device name. Names use the existing UTF-8 codec and 40-byte database limit. There is no schema migration.
- A deliberate short TFT tap from Management/enrollment now returns to a fresh Access QR, so Phone #2 need not wait ninety seconds after registration. Hold-to-Management, a further hold-to-reset, and both reset confirmations are unchanged. Navigation alone never unlocks.
- Device revoke affects that record alone. Whole-User disable is separate; it denies all that User's devices without rewriting their individual states. Re-enabling a User does not reactivate revoked devices. Owner disable and first Owner Device revoke remain forbidden.
- Enrollment consumption precedes authentication/device writes. Interrupted writes fail closed and may leave an orphan verifier, which cannot authorize without an active Device record. A lost enrollment success response is not falsely reported as success; a fresh grant may be needed.

## Storage and cloud

The existing microSD User/Device/auth files remain authoritative. This work does not erase, reset, re-register, migrate or replace the Owner. Tests with synthetic identities run only in host fakes. Local event segments retain their stable IDs, CRC validation and ACK-before-delete design. STA-only is added as a supported network-mode value while old AP/AP_STA records remain readable. The paused receiver accepts this additive mode; it still accepts only event fields, never credentials/verifiers/passwords/QR tokens.

Cloud is disabled on this board. The prototype implementation is retained behind an explicitly paused UI section. A future Google login/consent product flow would require separate design and authorization; manual Apps Script deployment is not the current user task.

## Verification evidence

Completed before hardware acceptance:

- Production NetworkManager with isolated Wi-Fi/NVS fakes: **101 assertions / 8 scenarios PASS**, including old-connected candidate safety, confirmed-only persistence, recovery retry, LAN-verification grace, AP-off stability and STA-only scanning.
- Production EnrollmentManager/AccessController bodies, unchanged production SessionManager and RecordCodec with host storage/auth/lock fakes: **163 assertions PASS**. Includes role boundaries, replay/expiry/type separation, issuer revocation, UTF-8 names, credential binding, disabled/revoked denial, correct audit identity, retained revocation after controller recreation and config/storage/timer failure. Auth hashing and physical SD power-loss behavior are not claimed by these fakes.
- Production enrollment browser script in independent fake storage contexts with WebCrypto: PASS for different credentials, correct device IDs, Thai name submission, no Admin secret in requests, expired grant and oversized UTF-8 name rejection.
- All eight embedded JavaScript assets pass syntax checks. Root independently reviewed Luna's UI, corrected permission-control mismatches and stale network copy, and verified the Thai Add User/role/QR-expiry display and registration success in a local browser preview with synthetic API responses. The preview QR is a layout fixture, not an optical scan test; no preview identity touches the ESP32.
- EventLog fake-SD regression plus new STA-mode serialization/readback PASS. Apps Script receiver mock regression plus STA-mode acceptance PASS; Google deployment remains paused.
- Final build PASS: RAM 119764/327680 bytes; flash 1244945/1310720 bytes (95.0%). No partition change. COM6 flash PASS in 83.13 seconds, hash verified.

Host fake tests are not physical magnet or real-phone evidence. No successful live Access request is sent by the automated preflight. Runtime evidence is appended below.

## Real Phone #2 test (pending)

1. Keep Owner's existing Chrome/profile and put both phones on home Wi-Fi. Hold the TFT about 1.5 seconds and scan MANAGEMENT in Owner's browser.
2. Open **ผู้ใช้**, confirm the existing Owner, press **+ เพิ่มผู้ใช้**, enter a name, choose **ผู้ใช้**, and create the enrollment QR.
3. Phone #2 scans that QR within two minutes, enters its device name and registers without an Admin password. Keep that same browser/profile for future Access scans.
4. Refresh Owner's dashboard/list: two Users and two ACTIVE Devices are expected if the board started with one each.
5. Single tap for a fresh ACCESS QR; Phone #2 scans. Confirm the magnet releases and then relocks on time.
6. Open a fresh MANAGEMENT session on Owner, select User #2's device and revoke that device only.
7. Phone #2 scans a fresh ACCESS QR: denied; magnet stays locked.
8. Owner scans another fresh ACCESS QR: Owner access still works and relocks.

Report each PASS/FAIL. Do not send credentials, enrollment tokens or passwords. Do not factory reset. After results, verify revocation persistence across an ordinary reboot and retain Phone #3 as a deferred regression.

## Final runtime and review result

The flashed checkpoint was rebooted through COM6 and observed for 35 seconds. Serial confirmed Configured YES, Owner YES, database PASS users=1 devices=1 active=1 revoked=0, SD OK, lock timer OK, GPIO22 LOCKED, session/state self-test PASS, canonical mDNS/HTTP started and saved STA connected at 192.168.1.179. Heap at startup: free 127612 bytes, minimum 121836. Cloud remained disabled/unconfigured. The absent optional cloud a/b configuration diagnostics are unchanged and unrelated to Owner storage.

The live LAN preflight passed health before and after the AP-close grace. Concurrent Serial observed `NETWORK: HOME LAN PRIMARY; AP OFF`. Invalid-token Add User, enrollment creation, revoke and User disable requests were denied. Invalid enrollment redemption and Access requests were denied. The final Thai Users/QR HTML was delivered. These are negative route tests; no real User/Device was provisioned and no successful unlock was requested.

After AP shutdown, direct multicast DNS advertised 192.168.1.179 for the canonical hostname and HTTP with that canonical Host passed. Windows OS name resolution still failed independently; this remains a Windows regression item and is not a board LAN failure. Phone-side hostname behavior on this build is part of the pending physical test.

USB audit checks passed: 11 retained segments, no storage fault, no dropped events, NTP synchronized, and the oldest two records retain their previous event IDs and valid CRCs. Owner/device authorization data was preserved. No reset, Owner replacement, production enrollment, passphrase/Wi-Fi change, cloud configuration, or Bluetooth implementation occurred.

Root/Sol independently reviewed the role/interface checks, issuer binding, one-use/session behavior, actual User audit attribution, revocation semantics, UTF-8 codec reuse, unchanged verifier storage, GPIO ownership and Luna UI/test changes. Host and negative-runtime verification: PASS. Real Phone #2 registration, optical QR scan, physical unlock/timed relock, revocation denial and Owner regression: PENDING. Commit as a tested implementation checkpoint, not as physical acceptance or Phase 12 cloud PASS. Stop here for the requested real Phone #2 test.
