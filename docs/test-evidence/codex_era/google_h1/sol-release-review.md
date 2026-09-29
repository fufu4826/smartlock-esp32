# SOL H1 release review

Release: c6c90af967d4bf361047b2e4bb435a0ac132fb37.

Reviewed the firmware changes to CloudSync, CloudTransport, CloudProtocol, CloudPairProtocol, GoogleReceiverConfig, Management JavaScript and cloud routes; receiver source matches the accepted version-4 SHA-256 a9784a7d718bce7e9969c4f943deb6ef7a129d053504909f23ce78d300eb6b08. Reviewed obsolete backend removals and accepted report/test scope. Unrelated final-from-zero/stale-enrollment edits are excluded. No new local authorization or lock writer is introduced.

- Cloud mutations retain fresh Owner Management authorization and home-LAN ingress checks.
- The fixed HTTPS endpoint is the accepted publisher deployment. Google CA verification and hostname validation remain enabled. One bodyless redirect is restricted to script.googleusercontent.com; installation proofs are POST-body fields.
- Worker networking is bounded, runs outside the main loop, and is gated on local readiness, locked state and available heap. Hardware timed relock remains independent of cloud work.
- A seven-field exact receipt, unchanged connection generation/key and re-read segment digest precede EventLog deletion. EventLog validates the segment again. Errors and uncertain responses do not authorize retirement.
- Cloud pairing/connection state uses its existing dedicated NVS namespace. Firmware changes do not modify Owner/identity/PIN/Wi-Fi/calibration stores or partition layout.
- Only LockController writes GPIO22. Its initialization is the first setup action and preloads HIGH before output enable. This source check does not establish absence of a physical electrical pulse during reset.
- Baseline current database is valid: ko/D000001 Owner ACTIVE and hi/D000002 User ACTIVE, two verifiers. OWNER_VERIFIER_PRESERVED=0 compares against a legacy migration archive; it is not current-database validity. No archive repair is authorized or necessary for deployment.
- Focused host checks passed: receiver 23, publisher pages 2, protocol 63, transport 55, sync/pair parser 44, cloud routes 43, browser VM 48; the separate Chromium Management fixture also passed. Hardware TLS remains a live gate.
- Build: PIO code size 1,250,553 bytes; binary 1,257,136 bytes; unchanged app slot 1,310,720 bytes. Binary headroom is 53,584 bytes. Static RAM is 118,276 bytes.
- Firmware SHA-256: f880bd22a293abbd550a34dd2122d785ee2537da501ff55653e5cde8aeda4860.
- Built partition table SHA-256: 148b959cbff1c38aa8e1d5c0ba9d612c54997b945e56a63f41223eef650653a1.

Source/config/build release gate: APPROVED. Application upload requires the separate live partition/boot-target verification gate. No erase-all, partition/bootloader/NVS/SD write or Factory Reset is approved. Historical evidence whitespace is retained to preserve source/evidence hashes.
