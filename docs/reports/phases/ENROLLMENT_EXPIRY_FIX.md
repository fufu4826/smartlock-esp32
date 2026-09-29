# Enrollment session immediate-expiry fix

Date: 2026-09-26

User evidence: Owner web page displays Boom/User enrollment QR and countdown; invited phone reports invalid/expired before showing assigned name/role.

The supplied QR was decoded read-only. It contains the expected canonical host, /enroll path and a 64-character session value. The token was not printed, logged or used to enroll.

## Root cause

main.loop captures `now` before web.handleClient(). The enrollment handler creates a session at a later millis() value. The old end-of-loop `sessions.expireSessions(now)` compares that new session with an older time. Unsigned subtraction wraps to a very large elapsed value, so the new enrollment is immediately invalidated. The client countdown is independent and can continue showing remaining time.

## Fix / review

The end-of-loop expiry sweep now uses `millis()` at the point of checking. Token generation, strict 120-second expiry, one-use consumption, issuer/role checks, revocation and lock authorization are unchanged. No time tolerance or permission bypass was introduced. Sol reviewed the stale-clock source and the single-line behavioral change.

## Tests

The production SessionManager regression reproduces the old failure with loop time 100 and creation time 125, then verifies the fresh-time call preserves the session, validity at 119999 ms and rejection at exactly 120000 ms. The test runner also checks the production main loop retains the fresh-time invocation. Full identity/controller tests: 343 assertions PASS, including the existing 20 migration fault boundaries, replay and revocation cases.

Physical invited-phone validation remains pending. No live identity was created for testing, and no Factory Reset or Owner re-registration is required. After firmware reboot, use a fresh Management QR and generate a fresh enrollment QR; pre-reboot RAM sessions cannot remain valid.

COM6 build/upload PASS (87.82 s), flashed hashes verified. Post-flash USB diagnostic: database valid, one active D000001/gugy Owner, one verifier, OWNER_VERIFIER_PRESERVED=1. Free heap 134,972 bytes; minimum 122,216. No new identity was created by the diagnostic; phone registration remains pending.
