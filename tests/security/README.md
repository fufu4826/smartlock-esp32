# Request and lock security host tests

Run with `python tests/security/run_security_policy_tests.py`.

The C++ test compiles the production `src/security/RequestPolicy.h` directly. It checks exact form content-type parsing, canonical device IDs and UTF-8 validation. Only LockController writes GPIO22. Approved unlock callers are AccessController and the PIN-authenticated physical Admin confirmation path in main.cpp. HTTP forms and device IDs use the production policy helper.

These are host/source checks. They do not exercise the HTTP server runtime, physical GPIO electrical levels, boot/reset behavior, management or enrollment routes end-to-end, backup/restore/OTA, or real network clients. GPIO checks establish current source ownership only.

## Deferred Phase 14 items from the 2026-09-26 user instruction

Keep these pending; do not report them as PASS:

1. boom / D000002 revocation persistence after reboot
2. Phone #3 physical enrollment/revoke test
3. Historical Google integration acceptance is archived; it is outside the active product scope.
4. Windows hostname/HTTP reliability regression
5. Changed-DHCP regression if still unverified
6. Complete Owner Access physical regression
7. Phone #2 enrollment/access/revoke regression
8. Factory Reset physical flow
9. Recovery AP physical behavior
10. Backup/Restore/browser OTA are removed from the product; historical acceptance is not a current regression gate.

Phase 14 remaining physical checks were subsequently USER WAIVED / NOT TESTED. Physical Admin acceptance is PASS. Final from-zero acceptance remains a separate, prepared-only cycle; do not execute Factory Reset automatically.
