# SOL deployment review gate

Review result: PASS for default safe production deployment; real cloud activation remains external-setup gated.

Reviewed EventLog/Audit snapshots, backward-compatible CRC/schema handling, installation metadata, protected retention/loss accounting, Factory Reset namespace definitions, CloudSync persisted state/worker ownership, bounded transport and parser, exact ACK/retirement, Owner/home-LAN routes and Thai UI, SQL migrations/reservations/locks, OAuth proofs/token vault, RAW fixed-range Sheets/readback, archive placement, production configuration and test changes. No new GPIO writer or backend unlock route exists. Worker has no identity/SD/TFT/lock authority. Network work is priority 0; main owner alone validates/pins/retires segments.

Internal corrections included PostgreSQL nested-lock transaction handling, contiguous Sheets ranges, NVS polling wear, fresh status scheduling, asynchronous DNS callback lifetime, HTTP/TLS absolute deadlines, backend failure backoff and durable spreadsheet-create claims. Earlier compile/test iterations are retained as evidence where available; none is counted as an acceptance PASS.

Acceptance evidence: 80/80 backend tests (original 70 preserved), all 21 integrated host/browser commands exit 0, default/full-cloud builds pass, source ownership checks and diff whitespace check pass. Native PostgreSQL/verified loopback HTTPS and Google provider fakes are distinguished from actual Google. Reset tests are host-only.

Default binary SHA256 E13CE7E0A50326E9D851E9ED6836C1EAE74D7F35C6B8EBC0AEB85AE0A949DF17, 1,115,824 bytes; static RAM 107,336. Full-cloud resource probe SHA256 F56FA80DA746D85C5F7361B12BC7F2EEB9E915E461CF6DB5D8E97FDBEF51ADFF, 1,271,792 bytes; static RAM 108,476; actual binary headroom 38,928. Partition layout unchanged. Placeholder resource origin must never be flashed.

Preflash live evidence preserves ko/D000001 Owner ACTIVE and hi/D000002 USER ACTIVE, two verifiers, healthy PIN store, configured Wi-Fi, 5-second unlock duration, calibration, SD and locked GPIO22. No Factory Reset or credential changes are authorized or required. Normal COM6 upload/reboot approved after this gate.

Post-deployment backend-only review: escaped JSON field names are rejected before pairing parse, expired rate-limit entries are pruned before capacity rejection, and PostgreSQL URL options cannot override verified TLS. Added actual OAuth/request boundary and database-policy tests. Final full backend suite is 83/83 PASS. These changes do not alter the flashed firmware image. SOL review PASS for the committed software, with real Google staging/active ESP32 TLS measurements still pending external setup.
