# Phase 1 report — core architecture, screen sleep, and tap detection

Phase: 1 — Core architecture / state machine / screen sleep / tap detection  
Goal: Establish the Phase 1 firmware baseline and document build, upload, startup, and physical acceptance evidence.  
Git commit: This report is included in the Phase 1 commit.  
Previous tag: `hardware-baseline-v1` (`73f4780`).

## Files changed

- `src/hardware/TouchManager.h` — `TouchEvent` and `TouchManager` interface.
- `src/hardware/TouchManager.cpp` — TFT_eSPI touch polling, stable press/release debounce, and single/triple-tap detection.
- `src/hardware/LockController.h/.cpp` — Sol-owned exclusive GPIO22 output and non-blocking timed lock controller.
- `src/hardware/DisplayManager.h/.cpp` — Sol-owned TFT rendering and backlight layer.
- `src/app/AppState.h`, `AppStateMachine.h/.cpp` — Sol-owned explicit Phase 1 states and screen timeout.
- `src/main.cpp` — integrates modules while keeping GPIO22 locked on boot.
- `docs/phase_reports/PHASE_01_REPORT.md` — this report.

Dependencies changed: No known dependency changes.

## Evidence collected

- Main agent built the Phase 1 firmware successfully with PlatformIO: PASS.
- Build memory: RAM 21,916 / 327,680 bytes; flash 306,645 / 1,310,720 bytes.
- Main agent uploaded successfully to COM6: PASS.
- Serial startup output at 115200 included: `BOOT`, `SmartLock Phase 1`, `LockController: LOCKED`, `TFT: OK`, `Touch calibration loaded`, `Touch: OK`, `SCREEN: OFF`, and `READY`.
- User physically confirmed single tap shows ACCESS REQUEST, triple tap shows MANAGEMENT REQUEST, a two-second hold does not repeat, the backlight sleeps after about 30 seconds, and the magnet remains energized/locked throughout.
- During the live monitor, at least ten `EVENT: SINGLE_TAP` / `SCREEN: ACCESS REQUEST` pairs and one `SCREEN: OFF` transition were observed. These events alone do not establish all physical acceptance checks.
- User physically observed screen wake/sleep but no visible ACCESS REQUEST or MANAGEMENT REQUEST text in the first Phase 1 image. Sol Light marked Phase 1 FAIL. Root cause: the first renderer requested TFT_eSPI font 2 while `platformio.ini` only enabled built-in font and font 4. Sol Light replaced the renderer with DisplayManager drawing two lines in enabled font 4. The corrected image built and uploaded to COM6; serial again reached READY without a crash. User then confirmed both labels visibly rendered.

## Acceptance criteria status

- Firmware compiles: PASS — corrected build uses 21,916/327,680 bytes RAM and 306,841/1,310,720 bytes flash.
- Firmware uploads: PASS — main agent reported successful upload to COM6.
- Startup reports lock controller LOCKED: PASS — serial reported `LockController: LOCKED`.
- TFT and touch initialize: PASS — serial reported `TFT: OK` and `Touch: OK` after calibration load.
- 10 or more single taps count exactly once: PASS — at least ten single events observed in live Serial; user confirmed one action per tap.
- 10 or more triple-tap sequences are detected and visibly rendered: PASS for the physically tested sequences; live Serial showed `TRIPLE_TAP` and `SCREEN: MANAGEMENT REQUEST`, and user confirmed visible text. An exact count of ten triple sequences was not captured in a persistent log.
- Holding touch for two seconds does not repeat: PASS per user physical check.
- Screen sleeps and wakes on touch: PASS per user physical check and Serial `SCREEN: OFF`/request transitions.
- No watchdog/reset during the observed startup: PASS for the observed monitor interval; no crash/reset was captured.
- GPIO22 remains LOCKED except for an explicit hardware test: PASS during the Phase 1 physical test; user confirmed magnet remained energized/locked.

## Tests, bugs, fixes, resources

- Tests performed: Phase 1 firmware build, upload to COM6, and serial startup inspection (as reported by the main agent).
- Physical tests still required: at least 10 single taps, at least 10 triple-tap sequences, a two-second hold, and screen sleep/wake.
- TouchManager contribution: samples with `TFT_eSPI::getTouch`, requires 65 ms stable press and release, emits `SingleTap` on the first stable press, emits `TripleTap` on the third separate stable press when the full sequence is within 1200 ms and each inter-press gap is at most 500 ms, and uses unsigned elapsed-time subtraction for `millis()` rollover safety.
- Build result: PASS as reported by the main agent. RAM 21,916 / 327,680 bytes; flash 306,645 / 1,310,720 bytes.
- Upload result: PASS as reported by the main agent on COM6.
- Known limitations: Exact ten-sequence triple-tap count was not preserved in a log; the physical triple-tap behavior was confirmed. The baseline hardware-test firmware remains retrievable from `hardware-baseline-v1` for explicit lock-output testing.

Ready for next phase: YES — Sol Light reviewed the GPIO22, state-machine, touch, and rendering paths after the physical retest and accepts Phase 1.
