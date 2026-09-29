# Lead review gate — APPROVED FOR APPLICATION-ONLY COM6 DEPLOYMENT

Reviewed after the 16-run focused regression completed successfully and the release build succeeded.

- Reconciliation proves the exact pending credential against every persisted salted verifier, requires a unique matching ACTIVE identity, binds Setup to D000001/Owner, excludes Owner from Enrollment, and never mutates identities/verifiers or reuses an invitation. Enrollment reconciliation is LAN-only. Credentials stay in bounded POST bodies; query-bearing reconciliation is rejected.
- Setup activation updates credential authority only on success; retries preserve pending proof. Failed reconciliation leaves existing active storage intact. Fresh invitations can recover an abandoned pending workflow without reviving its identity. Reviewed the first-visit migration of legacy pending Setup storage and tested it.
- Management bearer authority has its own SessionManager slot, permanent observed-expiry retirement, issuer validation, replacement semantics, and no persistence. A wrong supplied bearer does not log out the valid bearer. The test explicitly checks the apparent valid window after a complete uint32 wrap.
- One five-second deadline spans parsing of the request line, headers and bounded body. No per-chunk renewal. Empty bodies do not allocate; existing body/header/MIME policies remain.
- TFT creation uses fresh millis. Browser deadlines start conservatively at request initiation and recompute on focus/visibility/callback. Server expiry and consumption are unchanged.
- EventLog retains at most 64 normal segments, evicts the oldest validated eligible closed segment, and persists conservative loss evidence before removal. Export names are canonical and validated; ordinary reads do not pin history. New claimed/source columns remain CRC-covered and legacy rows remain readable. Dormant cloud serialization is compatible but remains disabled and unconfigured.
- Access denial distinguishes claimed and verified identities; malformed identifiers cannot carry secrets into logs. Ingress is based on the socket interface. Only successful physical emergency unlock arms its relock audit event.
- Cross-fix review covered registration/credential precedence, all Management mutation gates, HTTP blocking/TFT timing, and retention/richer audit rows.
- GPIO22 still has exactly one writer: LockController. Its HIGH=LOCKED / LOW=UNLOCKED levels, timer and boot sequence are unchanged. The only unlock callers remain authorized Access and confirmed physical ADMIN Emergency Unlock.
- No partition/build-setting/library changes. FactoryReset only learns the newly added temporary slot/counter namespace; no reset was performed or authorized by this gate.

Release binary: **1,257,216 bytes**; application slot: **1,310,720 bytes**; conservative binary headroom: **53,504 bytes**. Growth: **17,008 bytes**. PlatformIO program size: 1,250,641 bytes. Static RAM: **108,556 bytes**, +88 bytes from baseline.

Firmware SHA-256: `13fab55330adb9b981f4b194dfb82bd7883a3016e49f6032f71cc89e5fff4805`.

Deploy only this application image to app0 at 0x10000. Do not write the partition table, bootloader, NVS, otadata, SD databases, credentials, or browser storage. Normal boot and read-only diagnostics follow. Physical magnet movement and phone/PIN interaction cannot be certified remotely.
