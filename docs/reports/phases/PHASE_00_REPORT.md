# Phase 0 report — hardware baseline

Phase: 0 — Freeze and verify hardware baseline  
Goal: Record the existing ESP32-035 display, touch, calibration, GPIO22, polarity, and dependency baseline before implementation work.  
Git commit: This report is included in the tagged baseline commit.  
Git tag: `hardware-baseline-v1`.

## Files changed

- `docs/HARDWARE_BASELINE.md` — configuration and evidence inventory.
- `docs/phase_reports/PHASE_00_REPORT.md` — this draft report.

Dependencies changed: No.

## Evidence collected

- Inspected `platformio.ini`, `src/main.cpp`, and `README.md`.
- Resolved package listing: PlatformIO `espressif32` 6.12.0; Arduino framework package 3.20017.241212+sha.dcc1105b; TFT_eSPI 2.5.43; Xtensa toolchain 8.4.0+2021r2-patch5.
- Main agent rebuilt the unchanged source with `pio run`: PASS. RAM 21,868/327,680 bytes (6.7%); flash 306,717/1,310,720 bytes (23.4%).
- Main agent uploaded with `pio run -t upload --upload-port COM6`: PASS; ESP32-D0WD-V3 rev 3.1, image hash verified.
- Serial at 115200: `BOOT`, `GPIO22 configured`, `LOCK STATE: LOCKED`, `TFT initialized`, `Touch initialized`, `Touch calibration loaded`, `READY`. No crash or reset loop appeared in 18 seconds.
- `pio pkg list` initially hit a Windows console encoding error; rerunning with `PYTHONIOENCODING=utf-8` completed successfully.

## Acceptance criteria status

- Baseline compiles: PASS — main agent ran `pio run` against unchanged source.
- Baseline flashes: PASS — main agent uploaded to the identified COM6 device.
- Serial has no reset loop: PASS for the 18-second capture; no exception observed.
- TFT initializes: PASS as reported by runtime initialization; visible screen output awaits human confirmation.
- Touch initializes: PASS as reported by runtime initialization; correct touch response awaits human confirmation.
- GPIO22 starts in LOCKED level: PASS from source initialization and serial state; GPIO22 before `setup()` was not measured.
- Existing lock UI functions: PASS — user physically observed green UNLOCK → red LOCK → green UNLOCK, single action per touch, magnet release then energization.
- Exact source state committed/tagged: PASS after `hardware-baseline-v1` is created.

## Tests, bugs, fixes, resources

- Tests performed: source/configuration inspection, PlatformIO dependency listing, build, flash, and serial observation.
- Bugs found: None asserted. Documentation/configuration discrepancy recorded: GPIO36 is mentioned as touch IRQ in README/master plan but is not configured or used by the current polling firmware.
- Fixes made: Documentation only.
- COM port: COM6, present CH340 USB-SERIAL; esptool identified the ESP32-D0WD-V3.
- Flash/RAM usage: 23.4% / 6.7% of the PlatformIO application limits.
- Known limitations: current NVS calibration contents and GPIO22 electrical level before `setup()` are unmeasured. Software cannot enforce the magnetic-lock state before firmware starts.

Ready for next phase: YES — all Phase 0 acceptance criteria passed. The physical result was reported by the user; the main agent independently reviewed the source, build, flash, and serial results.
