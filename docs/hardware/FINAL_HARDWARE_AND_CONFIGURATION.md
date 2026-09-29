# Final hardware and software configuration (source-grounded)

This document records configuration from the accepted project source/configuration and saved baseline reports. It does not establish the disconnected board's present state. No credentials or secret values are included.

| Area | Configuration | Evidence |
|---|---|---|
| Board target | PlatformIO `esp32dev`; project identifies ESP32-035 / classic ESP32 | `platformio.ini`; `docs/HARDWARE_BASELINE.md` |
| Build environment | PlatformIO `espressif32@6.12.0`; Arduino framework; configured env `esp32_035` | `platformio.ini` |
| Framework/tool versions recorded | Arduino ESP32 package `3.20017.241212+sha.dcc1105b` (2.0.17 package line); TFT_eSPI 2.5.43; Xtensa toolchain 8.4.0+2021r2-patch5; esptoolpy 2.40900.250804 | `docs/HARDWARE_BASELINE.md` (resolved package list; historical environment observation) |
| Display | 3.5-inch ST7796, 320×480, portrait configuration; shared HSPI | `platformio.ini`; hardware baseline |
| TFT pins | MISO 12, MOSI 13, SCLK 14, CS 15, DC 2, reset -1, backlight 27 active HIGH | `platformio.ini` |
| Touch | XPT2046 via TFT_eSPI polling on shared HSPI; touch CS 33; 2.5 MHz | `platformio.ini`; hardware baseline |
| SD storage interface | SD initialized on CS GPIO5 using a dedicated SPI object at 4 MHz | `src/storage/StorageHealth.cpp` |
| Lock output | GPIO22; locked level HIGH, unlocked level LOW; controller starts locked and applies timed relock | `src/hardware/LockController.h/.cpp` |
| Network | Startup protected AP; STA can be used with stored home network; AP+STA and protected local fallback behavior are implemented in `NetworkManager` | `src/network/NetworkManager.cpp`; final acceptance report |
| Canonical hostname | Hardware-derived `smartlock-<48-bit efuse MAC>.local`; accepted phone path recorded as `smartlock-04225a0ff0a4.local` | `src/network/CanonicalOrigin.cpp`; final acceptance report |
| Persistent data | Installation/configuration and calibration/PIN settings use NVS/Preferences; identity and authorization stores use SD-backed files; preserve existing state | Storage modules; final acceptance report; architecture reports |
| LINE notification architecture | RAM queue/worker with TLS API calls; Owner-gated configuration/maintenance path; no credential values copied here | `LINE_SECURITY_NOTIFICATIONS_ARCHITECTURE.md`; source modules and final acceptance report |
| Production commit/tag | `d1e5ce38058790a973ce90c1addf33ef206ce4cd`; tag `smartlock-final-accepted-2026-09-28` | Git exports in `09_GIT_AND_RELEASE_HISTORY` |
| Accepted firmware | 1,238,752 bytes; SHA-256 `084227e2e77994ae21b773e253c209b8ba49b897354b3969a5f76f328cb06be9` | Archived `accepted_firmware/firmware.bin`; release report |

The current firmware configuration is source evidence; hardware actions and package versions are historical evidence from the saved reports. This archive task did not connect to the board or rebuild firmware.