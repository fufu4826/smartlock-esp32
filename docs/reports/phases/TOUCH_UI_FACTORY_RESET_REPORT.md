# Touch gestures, QR colors, and Owner-confirmed factory reset

## Latest user-requested revision (supersedes the behavior below)

- Hold once from ACCESS to enter yellow MANAGEMENT. Release, then hold again on MANAGEMENT to enter red RESET.
- RESET contains no QR. It shows Thai Yes/No buttons; No returns to ACCESS, Yes opens a separate OK confirmation screen.
- Reset executes only on a new short tap and release within the OK button. The hold that opens RESET cannot choose Yes, and the Yes tap cannot also press OK.
- The physical OK path replaces Owner verification in the web page, as explicitly requested by the user. Reset web routes are no longer registered.
- GPIO22 is locked before erasing SD/NVS. Owner, device credentials, SmartLock records/backups/logs, config, and saved AP/STA settings are removed. Hardware touch calibration remains.
- Final build and COM6 flash PASS (RAM 37.4%, flash 74.7%, image hash verified). Thai prompt preview inspected.
- Serial after final flash: configured YES, Owner YES, SD OK, one active device, session/state-machine self-test PASS, lock timer OK, GPIO22 LOCKED. The saved registration was preserved by flashing; reset was not automatically exercised.
- Physical acceptance pending: hold/release/hold, No returns to ACCESS, Yes shows separate OK, and intentional OK returns to initial setup while magnet remains locked.

Status: firmware implemented and flashed; physical display, touch, and reset confirmation checks pending.

## Behavior

- A single tap displays ACCESS QR on a white screen when configured, or SETUP QR when unconfigured.
- A continuous 1.5-second press displays MANAGEMENT QR on a yellow screen. A hold fires once per press.
- Three separate quick taps display RESET QR on a red screen. Holding the third tap does not switch it to Management.
- Each QR symbol, including its quiet zone, remains black on a white square regardless of the screen color.
- RESET QR opens a page that requires an active Owner browser credential, an explicit checkbox, and a second confirmation. The reset session is random, expires after 90 seconds, and is consumed once. Merely opening the red screen or scanning its QR cannot erase data.
- A confirmed reset keeps GPIO22 locked, removes `/smartlock` from microSD, clears SmartLock config, AP/STA secrets, and test state in NVS, then clears saved Wi-Fi credentials and restarts into initial setup. Touch calibration is kept as hardware calibration.

## Evidence

- `pio run -e esp32_035`: PASS (RAM 37.4%, flash 74.7%).
- `node scripts/phase6_asset_syntax.js`: all seven embedded pages PASS.
- `pio run -e esp32_035 -t upload --upload-port COM6`: PASS, image SHA verified.
- Serial after flash: `LockController: LOCKED`, `LOCK TIMER: OK`, `Configured: NO`, `Owner exists: NO`, touch calibration loaded, SD OK, session self-test PASS, AP/DNS/HTTP OK.
- The board is currently unconfigured following the user-requested clean reset. A physical Owner-confirmed reset has not been run against a newly configured installation.

## Physical acceptance pending

- After first Owner setup, verify the white ACCESS, yellow MANAGEMENT on hold, and red RESET on triple tap. Scan each QR and confirm its code remains on white.
- Confirm a hold does not repeatedly trigger actions and a held third tap stays on RESET.
- Before confirming reset, verify the magnet remains locked and unregistered/non-Owner browsers cannot reset.
- If the Owner chooses to confirm reset, verify reboot into `Configured: NO`, `Owner exists: NO`, setup AP, and locked magnet.
