# SmartLock post-acceptance cleanup and documentation archive

Date: 2026-09-28

The accepted source baseline remains Git tag `smartlock-final-accepted-2026-09-28` at commit `d1e5ce38058790a973ce90c1addf33ef206ce4cd`. The accepted firmware digest remains `084227e2e77994ae21b773e253c209b8ba49b897354b3969a5f76f328cb06be9`. The verified rollback backup at `D:\SmartLock_Backups\SmartLock_FINAL_ACCEPTED_2026-09-28_d1e5ce3` was not modified.

The documentation archive contains 407 files after adding clean-build validation artifacts and final manifests; the exact sizes and hashes are recorded in `manifest/DOCUMENTATION_MANIFEST.csv`. It includes all 328 `docs/` files, 259 raw evidence files, 58 phase reports, six images, Git/release exports, five chapter indexes, the final test matrix, and the pre-cleanup inventory. The archive worker recorded 375 of 375 source-to-copy SHA-256 checks passing, with zero unknown inventory rows and 29 retained review-required rows. The accepted firmware copy is independently present under `09_GIT_AND_RELEASE_HISTORY/accepted_firmware/firmware.bin`.

The active checkout was reduced to runtime source, libraries, `platformio.ini`, `.gitignore`, README, and development tests. Historical reports, generated PlatformIO output, reset utility source, probe scripts, bitmap tools, monitor helper, and empty support directories were removed only after archive verification. No source logic, accepted tag, or accepted firmware was changed.

Offline validation passed with `pio run -e esp32_035`: flash 1,232,177/1,310,720 bytes (94.0%) and RAM 112,612/327,680 bytes (34.4%). The clean-build binary is a validation artifact under `09_GIT_AND_RELEASE_HISTORY/clean_rebuild_validation`; it is not substituted for the accepted release image.

Passing host checks were line notification, admin, security policy, canonical origin, network manager, registration/AuthStore, identity, HTTP deadline, and touch/state regression. The legacy event-log and phone2 suites remain deferred because they reference absent `src/events/EventLog.h` and `src/events/EventLog.cpp`; no firmware source was changed to repair those historical test harnesses. No upload, reset, serial monitor, or COM-port operation occurred because the board was physically disconnected.

The remaining cleanup evidence is `CLEAN_RUNTIME_MANIFEST.csv`, generated after this report and the clean-build artifacts were archived.
