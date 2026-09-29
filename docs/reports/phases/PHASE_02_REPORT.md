# Phase 2 report — versioned NVS and microSD database foundation

Phase: 2  
Goal: Create fail-closed boot configuration and persistent bounded user/device stores.  
Git commit: This report is included in the Phase 2 commit.  
Previous tag: `phase-1-verified`.

## Files and dependencies

- Added `src/storage/ConfigStore.h/.cpp`, `StorageHealth.h/.cpp`, `AtomicFileStore.h/.cpp`, `RecordCodec.h/.cpp`, `UserStore.h/.cpp`, `DeviceStore.h/.cpp`, and one-time `Phase2SelfTest.h/.cpp`.
- Updated `src/main.cpp`, `platformio.ini`, `README.md`, and `docs/DATABASE_SCHEMA.md`.
- Used ESP32 core `Preferences`, `SD`, and `SPI`; no new third-party library or Bluetooth code.
- Luna High implemented the bounded record codec and initial user/device wrappers. Sol Light reviewed and corrected the write paths, corruption handling, and diagnostics.

## Build, flash, runtime

- Final build: PASS (`pio run`). Flash 368,653/1,310,720 bytes (28.1%); static RAM 47,236/327,680 bytes (14.4%).
- Final flash: PASS (`pio run -t upload --upload-port COM6`), image hash verified on ESP32-D0WD-V3.
- Final serial boot at 115200: `BOOT`, `SmartLock Phase 2`, `LockController: LOCKED`, `Config schema: 1`, `Configured: NO`, `Owner exists: NO`, `TFT: OK`, `Touch calibration loaded`, `Touch: OK`, `SD: OK (30000 MB)`, `SCREEN: OFF`, `READY`. No exception/reset loop appeared in the observation window.
- Before a card was inserted, the same mount path printed `SD: FAIL (mount/no card)` and continued to `READY` with the lock controller reporting LOCKED.

## Acceptance evidence

- NVS survives restart: PASS. Schema 1 defaults were created once and loaded on later boot; configured and owner flags remain NO.
- SD structure: PASS. The firmware created `/smartlock/` and its `db`, `logs`, `backups`, and `export` directories on a 30,000 MB card.
- Multiple users/devices, reload, modification, reboot persistence, and removal: PASS. The gated one-time on-device test created two users and two devices, reloaded them after reboot, modified and reloaded after another reboot, then replaced both stores with empty files. Its NVS stage reached 4 only after these steps succeeded; stage 4 confirmed empty current databases and removed test backup files.
- Missing SD: PASS for graceful boot and locked software state; no crash was observed.
- Corrupt file: PASS in the reserved test path. The test appended a byte to a CRC-protected file, confirmed `AtomicFileStore::read` returned `Corrupt`, and removed that exact probe file.
- Database failures fail closed: source review confirms readers reject corrupt current files and never use `.bak` for authentication; writers reject corrupt envelope or logical records. No Phase 2 code calls `LockController::unlock()`.
- Plaintext secret logging: PASS by source review; Phase 2 creates no credentials and serial prints only status/test stage.
- Physical magnet still locked with final SD image: PASS. User confirmed magnet energized/door locked with microSD inserted, ACCESS REQUEST visible after one tap, and no unintended unlock during or after the tap.

## Review and limitations

- GPIO22 remains exclusively in `LockController`. Main calls `begin()` before NVS, TFT, touch, and SD initialization. No storage path writes GPIO22.
- A valid `.bak` with an invalid/missing current file is retained for diagnosis but never trusted for access, to avoid resurrecting a revoked credential. No automatic migration is implemented.
- Store buffers are bounded; current APIs are synchronous and single-task only. Future web handlers must dispatch storage operations serially or add locking.
- The exact storage self-test transcript during its automatic restarts was not captured, but its persistent completion marker and final empty-database readback were observed. Temporary `PHASE2_SELFTEST` is absent from the final build flags.

Ready for next phase: YES — Sol Light reviewed the NVS schema, atomic write/recovery policy, GPIO22 isolation, final build/flash/Serial evidence, on-device persistence tests, and the user's physical lock confirmation.
