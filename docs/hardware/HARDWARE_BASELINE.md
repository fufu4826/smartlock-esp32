# ESP32-035 hardware baseline (Phase 0 candidate)

Status: source, build, upload, serial boot, and physical screen/touch/magnet behavior verified on 2026-09-25.

## Board and display

- Board in `platformio.ini`: PlatformIO `esp32dev`; project README and master plan identify the target board as ESP32-035 with a classic ESP32.
- Display: 3.5-inch, 320 × 480, portrait rotation 0; TFT_eSPI driver selected by `ST7796_DRIVER`.
- TFT_eSPI transport: HSPI, shared with touch.
- TFT pins: MISO GPIO12, MOSI GPIO13, SCLK GPIO14, CS GPIO15, DC GPIO2, reset `-1` (not controlled by a GPIO), backlight GPIO27, backlight active HIGH.
- SPI frequencies: TFT write 27 MHz, TFT read 16 MHz, touch 2.5 MHz.

## Touch

- Controller: XPT2046, using TFT_eSPI touch polling on the shared HSPI bus.
- Touch chip-select: GPIO33.
- IRQ: the README/master plan list GPIO36, but the current `platformio.ini` does not define a touch IRQ build flag and the firmware polls `tft.getTouch()`; GPIO36 is therefore not used by the current firmware path.
- Calibration is not hard-coded. On first boot, or when no correctly sized calibration record is present, firmware calls `tft.calibrateTouch(calibration, TFT_WHITE, TFT_BLACK, 15)` and stores five `uint16_t` values in ESP32 NVS (Preferences namespace `lock-touch-v2`, key `cal`). On later boots, the record is loaded and passed to `tft.setTouch()`.
- To request a fresh calibration, hold BOOT (GPIO0, active LOW) while resetting/powering the board; release after the calibration screen appears, then touch the on-screen calibration targets. Existing README says calibration is saved in NVS. The current stored calibration values were not read or copied into this report.

## Lock output and polarity

- Lock MOSFET control: GPIO22; firmware constants set `LOCKED_LEVEL = HIGH` and `UNLOCKED_LEVEL = LOW`.
- The plan and README describe HIGH → MOSFET on → magnet energized → locked; LOW → MOSFET off → magnet de-energized → unlocked. This is the documented intended polarity, not a measurement made during this documentation task.
- In `setup()`, source preloads GPIO22 HIGH, sets it OUTPUT, writes HIGH again, then logs the configured locked state. This is a source inspection only. GPIO22 is not under firmware control before `setup()` executes; the README notes that external pull circuitry is needed if state must be guaranteed across reset/startup.
- The current test firmware's centered touchscreen button can toggle the output between locked and unlocked. It is a hardware test sketch, not a production lock controller.

## Resolved software versions

Resolved with `pio pkg list -e esp32_035` for the configured environment:

| Component | Resolved version |
|---|---|
| PlatformIO platform `espressif32` | 6.12.0 |
| Arduino framework package `framework-arduinoespressif32` | 3.20017.241212+sha.dcc1105b (Arduino-ESP32 2.0.17 package line) |
| `TFT_eSPI` | 2.5.43 |
| Xtensa ESP32 toolchain | 8.4.0+2021r2-patch5 |
| `tool-esptoolpy` | 2.40900.250804 |

The project requests these directly in `platformio.ini`: `espressif32@6.12.0`, `bodmer/TFT_eSPI@2.5.43`, Arduino framework, and `esp32dev`. No other direct library dependency is declared.

## Verified evidence and limits

- Source of configuration: `platformio.ini`, `src/main.cpp`, and `README.md` in this project; hardware expectations are also listed in the ESP32-035 Smart Lock master plan, Phase 0 and hardware section.
- The main agent ran `pio run` successfully against this unchanged source (Flash 306,717/1,310,720 bytes; RAM 21,868/327,680 bytes), then `pio run -t upload --upload-port COM6`; esptool verified the flashed image hash on ESP32-D0WD-V3 revision 3.1 (CH340 USB serial device).
- A 115200 baud serial capture after upload showed `BOOT`, `GPIO22 configured`, `LOCK STATE: LOCKED`, `TFT initialized`, `Touch initialized`, `Touch calibration loaded`, and `READY`, with no reset loop or exception during an 18-second observation.
- The user confirmed one visible green UNLOCK button, one action per touch, red LOCK after unlocking, green UNLOCK after relocking, magnet release on UNLOCK, and magnet energization on LOCK. This confirms the practical touch/UI/output polarity test; no electrical waveform or GPIO22 level before `setup()` was measured. Stored calibration values were not read or copied.

## Configuration references

- `platformio.ini`
- `src/main.cpp`
- `README.md`
- `ESP32-035_SmartLock_Codex_Master_Plan_SOL_LIGHT_LUNA_HIGH.md` (Phase 0 and verified display/touch configuration sections)
