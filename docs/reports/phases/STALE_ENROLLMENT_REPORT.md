# Stale browser credential enrollment fix

Root cause: Enrollment blocked solely on credential-shaped localStorage values. After Factory Reset those values can be unknown to the current database.

Added POST /api/enroll/credential-status. It requires the normal configured/LAN/body/rate checks and a valid unexpired, unused Enrollment session whose issuer remains authorized and whose assigned name is available. It checks current IdentityStore health and AuthStore proof before returning only ACTIVE, REVOKED or UNKNOWN. Invalid invitations/storage/status transport errors deny progress. No credential is in a query, response, log or Serial message; request credential buffer is wiped. Existing endpoints either consume grants or authenticate Management, so neither safely served this read-only enrollment decision.

Enrollment checks the invitation first, then saved candidate device/setup credentials. ACTIVE blocks duplicate enrollment without changing storage. UNKNOWN/REVOKED proceed to the normal assigned-name/role screen. New independent random credential is kept in pending storage; old registered credential remains until successful completion. Only success replaces the browser credential list with the newly issued identity and removes stale Setup credentials. No automatic clearing on page open. Failure preserves old storage plus recoverable pending credential. Owner ACTIVE browser is blocked and untouched.

The existing completion transaction and authorization checks are unchanged: consume before credential write; orphan verifiers from interrupted identity writes cannot authorize. No old revoked identity is reactivated. This is not a new cross-file transaction scheme or a bypass of existing fail-closed recovery.

Focused tests PASS: production Enrollment/Access/Identity/Session methods with isolated host storage/auth dependencies; ACTIVE/UNKNOWN/REVOKED proof, same-ID wrong proof, expiry, consumed/replayed token, revoked issuer, new identity after revoke, old revocation retained, Owner unchanged, no enrollment unlock. Browser-context tests cover none/ACTIVE/UNKNOWN/REVOKED, failed status/invitation, random new credential, storage preservation on failed registration, successful replacement, Owner browser preservation, no extra name field and POST-body credentials. Management DOM/source GPIO policy PASS. No broad suites or live registration executed by Codex.

Production files: EnrollmentManager.cpp/.h, WebServerManager.cpp/.h, WebAssets.h. Focused tests: browser_identity.test.js, cleanup_smoke.cpp, stale_enrollment.test.js. One build; binary 1,240,208 bytes, headroom 70,512 bytes in unchanged 1,310,720-byte slot; static RAM 108,468 bytes. SHA-256 4dd2eb25db6f3828097eb2448327c4aa17f44620b0341fcf18ddf4329a1c1ee8.

Preflight: ko/D000001 is the only ACTIVE OWNER/identity and has one verifier. Deployment preservation evidence follows. Physical stale-phone retest pending. No reset, Owner recreation, PIN change, Wi-Fi configuration or Google deployment.

COM6 flash PASS with image hash verified; normal boot preservation PASS: GPIO22/LockController LOCKED, Configured YES, only ko/D000001 OWNER ACTIVE with one verifier, PIN store OK, SD/calibration OK, STA reconnected 192.168.1.179, canonical mDNS OK, no panic/watchdog. No data APIs were used to modify current authorization. OWNER_VERIFIER_PRESERVED=0 is the legacy migration-backup comparison on this fresh schema-2 installation, not evidence of current verifier loss.

Live updated /enroll page PASS. Empty status request rejected 400; valid-format random candidate with invalid invitation rejected 403. No live authorized invitation was created/consumed by Codex. Evidence in evidence/stale_enrollment. Full physical result remains PENDING: Owner creates fresh Enrollment QR; pre-reset Phone #2 browser opens it without clearing site data, registers, appears as new identity, and uses a fresh Access QR with timed relock. Google stays paused.

## Physical acceptance PASS

User confirmed old Phone #2 site storage was not cleared; fresh QR opened normally; stale credential did not block enrollment; new identity hi registered and appeared in Management; fresh Access authorized with physical magnet release and timed relock. Defect physically accepted. No additional reset authorized.
