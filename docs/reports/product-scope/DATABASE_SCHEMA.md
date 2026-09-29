# Phase 2 storage design (schema 1)

## Authority and failure behavior

GPIO22 is owned only by `LockController`. Storage reads and writes never unlock. A missing, corrupt, or unsupported database makes credential-dependent access unavailable and leaves the door locked. `configured` and `owner_exists` remain false until the full setup transaction in Phase 5; Phase 2 creates no trusted owner or credential.

## NVS

Namespace `sl-config`, schema version 1. Defaults: unconfigured, no owner, device name `SmartLock`, AP mode, unlock 10,000 ms, access QR 30,000 ms, management QR 90,000 ms, screen timeout 30,000 ms. Each value has a bounded type and explicit validity range. Unsupported future schema is an error, not silently reset. Existing `lock-touch-v2/cal` remains untouched. Network passwords are not part of Phase 2.

## microSD

The ESP32-035 uses a separate VSPI bus for SD: CS GPIO5, SCLK18, MISO19, MOSI23. Start at 4 MHz. TFT/touch remain on HSPI pins 12/13/14. The intended root is `/smartlock/`, with `db/`, `logs/`, `backups/`, and `export/`.

Phase 2 database files are `/smartlock/db/users.rec` and `/smartlock/db/devices.rec`. Each has a binary `SLDB` envelope with schema 1, bounded length (at most 8192 bytes), and CRC32. The payload contains newline-terminated records. User lines are `U1|id|name|ROLE|STATUS|CRC32`; device lines are `D1|id|userId|name|STATUS|CRC32`. The record CRC32 covers the line prefix through its last delimiter. IDs are bounded ASCII, names are bounded UTF-8, and roles/statuses use strict enumerations. Each store holds at most 64 records and rejects duplicates. No credential is created by Phase 2 diagnostics. Future schema migrations must be explicit.

## Atomic update and recovery

Each critical file has current, `.tmp`, and `.bak` paths. The writer creates and flushes `.tmp`, reopens and validates it, moves valid current to `.bak`, then promotes `.tmp` to current. On boot, the reader validates current. A valid backup with a missing/corrupt current is diagnostic evidence only; it is **not** used for authentication because doing so could resurrect a revoked device. A stray `.tmp` alone is never trusted as committed data. No parse error silently creates an empty database over existing files. The writer rejects replacement if the committed file fails envelope or logical record validation. All operations are currently synchronous on one task; concurrent web access will require locking or single-thread dispatch.

The two logical stores are not yet a multi-file transaction. Phase 5 must commit first-owner setup only after both user and device records validate on disk, then mark NVS configured/owner flags. Recovery from partial setup remains unconfigured and locked.

## Test data

Temporary Phase 2 records used reserved `UTEST`/`DTEST` IDs and contained no secrets. The test ran only on a fresh database, exercised create/reload/modify/reload/remove across reboots, then removed test records and backup copies. The final build does not define `PHASE2_SELFTEST`, so no diagnostic writes or test command paths are active. Storage tests only manipulated files inside `/smartlock/` and preserved any pre-existing database by refusing the diagnostic run.
