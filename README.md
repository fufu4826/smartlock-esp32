# ESP32-035 SmartLock

Prototype smart door lock on an **ESP32-035** board (3.5" ST7796 touch screen) that drives a magnetic lock. It includes:

- Access and Management via QR codes and the phone browser
- a physical Admin PIN on the touch screen
- LINE notifications, broadcast to every friend of one LINE Official Account

It runs fully locally: no cloud server, no webhook, no paid service.

> ภาษาไทยโดยย่อ: กลอนประตูอัจฉริยะต้นแบบบนบอร์ด ESP32-035 ใช้งานผ่าน QR และเบราว์เซอร์โทรศัพท์ มีรหัส Admin PIN บนหน้าจอ และแจ้งเตือนผ่าน LINE แบบ broadcast ไปยังเพื่อนทุกคนของบัญชี LINE Official Account เดียว ไม่ต้องใช้เซิร์ฟเวอร์ภายนอก

## Hardware

| Part | Connection |
|---|---|
| MCU | ESP32-D0WD-V3 (PlatformIO `esp32dev`, Arduino, `espressif32@6.12.0`) |
| Display | ST7796 320×480 portrait on HSPI: MISO12, MOSI13, SCLK14, CS15, DC2, BL27 |
| Touch | XPT2046 resistive on the same bus, CS33 (polling) |
| microSD | VSPI: CS5, SCLK18, MISO19, MOSI23 |
| Lock | MOSFET on **GPIO22**: HIGH = energized = LOCKED, LOW = released |
| BOOT button | GPIO0: hold 5 s after startup for protected network recovery |

`LockController` is the only production code that drives GPIO22. It sets the pin LOCKED at the very start of `setup()`. Hardware pull circuitry is needed if the lock state must be guaranteed before firmware runs.

## Build and flash

```bash
pio run -e esp32_035
pio run -e esp32_035 -t upload --upload-port COM6
pio device monitor -p COM6 -b 115200
```

For an application-only update, write `.pio/build/esp32_035/firmware.bin` to app0 at `0x10000`, then run `verify_flash`. This keeps NVS, the SD card and calibration intact.

## How it works

1. **First boot:** calibrate the touch screen. A blue **SETUP** QR appears; the Owner scans it and chooses an Owner name, an unlock duration (1–60 s) and a 4-digit Admin PIN. The browser keeps a generated credential; the device stores only verifiers.
2. **Access:** tap the screen to show the white **ACCESS** QR. A registered phone scans it, the lock releases for the configured time, then relocks automatically.
3. **Management:** hold for about 1.5 s to show the yellow **MANAGEMENT** QR, or open `http://smartlock-<id>.local` on the home Wi-Fi. The Owner can manage devices and roles, Wi-Fi, LINE, the unlock duration and the Admin PIN.
4. **Physical Admin:** open the Admin PIN menu on the TFT. From there you can do an Emergency timed unlock or a Factory Reset. Three wrong PINs lock the menu for 60 s.
5. **Factory Reset** clears the Owner, identities and auth (SD `/smartlock`), the Admin PIN, Wi-Fi and the setup/config state. It **keeps** touch calibration and the LINE configuration (prototype policy).

## LINE notifications (broadcast)

The device sends `UNLOCK_SUCCESS`, `ADMIN_PIN_FAILED`, `ADMIN_EMERGENCY_UNLOCK` and a test message with:

```
POST https://api.line.me/v2/bot/message/broadcast   (TLS, pinned root CA)
```

**Who receives them.** Every friend of the SmartLock LINE Official Account. To start receiving, press **เพิ่มเพื่อน LINE** on the Management LINE page; it opens `https://line.me/R/ti/p/<Basic ID>`. There is no per-user binding: anyone who adds the OA receives the notifications.

**Quota.** Broadcast messages count against the OA's monthly quota once **per recipient**. The LINE page shows the official quota and usage. If the quota runs out, only notifications stop; unlocking and relocking are never blocked by LINE.

**The token.** The Channel Access Token is stored only in device NVS (`sl-line`). It is never shown in the UI, a URL, logs or the browser. Initial LINE setup is an Owner + Admin PIN gated maintenance flow (`/api/line/maintenance/*`).

## Repository layout

This repository is the **complete sanitized project**: firmware source, released firmware, all reports, thesis materials and sanitized test evidence.

| Folder | Contents |
|---|---|
| `src/`, `lib/`, `platformio.ini` | Firmware source (build with `pio run -e esp32_035`) |
| `tests/` | Host tests |
| `firmware/latest/` | Accepted firmware `eaac772` (LINE broadcast) |
| `firmware/previous/` | Original accepted release `d1e5ce3` |
| `firmware/SHA256SUMS.txt`, `firmware/VERSION_INFO.md` | Hashes and flashing instructions |
| `docs/reports/claude/` | Claude reports: Factory Reset LINE preservation, Management settings, LINE broadcast, real-board acceptance |
| `docs/reports/phases/` | Codex phase reports (Phase 0–14, LINE, heap investigations; 46 reports). Obsolete Google Sheets and Backup/Restore/OTA (Phase 12–13) material was removed |
| `docs/reports/architecture/`, `docs/reports/product-scope/` | Architecture, design and product-scope documents |
| `docs/thesis/` | Source indexes for **chapters 1–5**, plus `FINAL_TEST_MATRIX.md`, `DEVELOPMENT_TIMELINE.md` and `IMPORTANT_COMMITS.md` |
| `docs/test-evidence/` | Sanitized evidence: Codex-era logs and JSON, Claude real-board serial logs (redacted), build evidence, screenshots |
| `docs/hardware/` | Hardware and configuration notes |
| `docs/release-history/` | Timeline, important commits, full commit chronology (no author emails) and tags |
| `docs/links/` | Where the shared Google Drive copy lives, and the access policy |
| `tools/` | Read-only diagnostic scripts and bitmap generators. The destructive `reset_tool` is intentionally **not** included. |

Chapter index files refer to old paths such as `docs/phase_reports/NAME.md`; the same file names are in `docs/reports/phases/`.

Source map:

```
src/app            state machine, access, enrollment, setup, physical admin, factory reset
src/hardware       LockController (GPIO22), display, touch
src/network        Wi-Fi/AP, mDNS canonical origin, captive portal, HTTP API (WebServerManager)
src/notifications  LINE broadcast worker (RAM queue, TLS, quota)
src/security       Admin PIN (PBKDF2 verifier, lockout), request policy
src/storage        ConfigStore, IdentityStore, AuthStore, atomic SD files, NVS secrets
src/web            Thai Setup / Access / Management pages (served from flash)
lib/SmartLockWebServer  bounded HTTP server
tests/             host tests (C++ fakes, Node, Python, Playwright)
```

## Tests

The tests run on a PC. The C++ suites need the Visual Studio C++ build tools; the JS suites need Node; the browser suite needs Playwright.

```bash
python tests/line/run_line_tests.py
python tests/line/factory_reset_line_preserve_test.py
python tests/admin/run_admin_tests.py
python tests/admin/line_maintenance_test.py
python tests/admin/line_management_browser_test.py
node tests/admin/web_test.js
python tests/security/run_security_policy_tests.py
python tests/identity/run_identity_tests.py
python tests/network/run_network_manager_tests.py
```

Known stale suites, kept for history:
- `tests/events/*` and `tests/phone2/*` reference the retired `EventLog` module.
- `tests/run_defect_fix_regression.py` no longer matches `main.cpp`.

## Versions

| Tag / commit | Meaning |
|---|---|
| `smartlock-final-accepted-2026-09-28` (`d1e5ce3`) | Original accepted release (fixed-recipient LINE push) |
| `523fd0b` | Factory Reset preserves the LINE configuration |
| `a5325b5` | Owner Management: LINE page, unlock duration, Admin PIN change |
| `eaac772` (tag `smartlock-line-broadcast-accepted-2026-09-28`) | LINE broadcast to all friends of the OA: **current accepted firmware** |
| `cd4c451` | Documentation only (README); no firmware change |

**Firmware lineage.** This GitHub repository is a clean source snapshot taken from the private development repository at `cd4c451`. Its own commit hash is **not** a firmware commit.

- The firmware source is identical to implementation commit `eaac772`.
- The flashed development build of `eaac772` has SHA-256 `546e96b03583eaf5405bbd7b1a6100cfee91937ba9047cc0026db4f4caa58e36`.
- Rebuilding in another folder changes only the embedded ELF hash and the appended image hash; the program code is the same.

## Status and known limitation

Development is **closed**. The user accepted the prototype in its current state.

- **Hardware-verified:**
  - Owner Access with timed relock
  - configurable unlock duration
  - Management Admin PIN change
  - Factory Reset, with LINE configuration preserved and fresh Setup afterwards
  - LINE broadcast to multiple OA friends
  - Add Friend
  - LINE Test
  - GPIO22 safety
- **Known accepted limitation:** a physical TFT Admin **Emergency Unlock** releases the lock correctly, but its LINE notification is not received. This was not fixed by decision.

## Security notes (prototype)

- No secrets are in this repository. The LINE token, Wi-Fi password, Admin PIN and browser credentials live only on the device or the phone.
- Anyone who is a friend of the OA receives notifications, including identity names in unlock messages.
- Factory Reset intentionally keeps the LINE token. Use Management → LINE → disconnect before handing the device over.
- **Sanitization.** Historical documents were copied with redactions:
  - personal emails, Google Apps Script and Sheet IDs, and the real LINE OA Basic ID appear as `<redacted-…>`
  - two Google Sheets screenshots were removed
  - raw git history is not published (author emails are omitted from `docs/release-history/SANITIZED_COMMIT_CHRONOLOGY.txt`)
- **Test fixtures** use obvious mock values, for example `mock_token_…`, `U0123456789abcdef…`, `owner-session` and PIN `2468`.

## Sharing

- **This repository is PRIVATE.** Access requires being a GitHub collaborator.
- **Primary share link:** a Google Drive folder `SmartLock_Project_Complete_2026-09-29` (anyone with the link can view and download). It has the same sanitized content plus one ZIP of everything. See `docs/links/`.
