# Owner Management web settings — implementation (2026-09-28)

Status: **post-acceptance prototype development.** Build and host tests pass. **Real-board acceptance is PENDING.**

| Item | Value |
|---|---|
| Base commit | `523fd0b` (Factory Reset preserves the prototype LINE configuration) |
| New commit | `a5325b5` "feat: complete owner management settings" |
| Old accepted tag | `smartlock-final-accepted-2026-09-28` still points to `d1e5ce3` (unchanged) |

No flash, Factory Reset or board-state change was performed.

## Scope

The goal is that the Owner can manage the prototype from a phone without developer tools. This package adds three things to Owner Management:

1. A single **LINE** page with the Add Friend button.
2. A **ระยะเวลาปลดล็อก** (unlock duration) page.
3. A **เปลี่ยนรหัส Admin PIN** (Admin PIN change) page.

Mobile navigation now reads: ภาพรวม · อุปกรณ์ · เครือข่าย · LINE · ระยะเวลาปลดล็อก · เปลี่ยนรหัส Admin PIN · ระบบ. The last three Owner pages are hidden for other roles, and direct `#hash` navigation to them falls back to the dashboard.

## Existing functionality reused

The package reuses existing server functions and adds no duplicate API:

| Area | Existing implementation reused |
|---|---|
| LINE status | `POST /api/line/status` (`lineOwnerAuthorized`) already returned `publicBasicId`, `configured`, `enabled` and `state`. |
| Admin PIN | `POST /api/system/admin-pin` → `ownerPinAuthorized()` → `AdminPin::change()` already existed. The form was buried in the "ระบบ" page and returned a single generic error. |
| Unlock duration | Single source of truth is `ConfigStore` → `SmartLockConfig.unlockDurationMs`. It is read live by `AccessController::request` and the physical Emergency path (`lockController.unlock(configStore.config().unlockDurationMs)`). Range: `ConfigStore::structurallyValid` requires 1000–60000 ms, and `FirstOwnerSetup` requires 1–60 s. There was no Management API or page. |

## Changes

### 1. LINE page (`src/web/WebAssets.h`)

- The former separate "การแจ้งเตือน LINE" and "เพิ่มเพื่อน LINE" pages are merged into one **LINE** page. It shows:
  - `LINE: พร้อมใช้งาน / ยังไม่พร้อม`, taken from `state === 'ready'`
  - `การแจ้งเตือน: เปิด / ปิด`
- **Add Friend button:** green (`#06c755`), full width and at least 48 px tall, labelled "เพิ่มเพื่อน LINE".
  - It is shown only when the public Basic ID matches `^@[A-Za-z0-9._-]{1,63}$`.
  - Its link is `https://line.me/R/ti/p/<encodeURIComponent(publicBasicId)>` and opens in a new tab with `noopener noreferrer`.
  - Its behaviour is unchanged. It is a plain link: no JS handler, no SmartLock request, no NVS write and no provisioning.
  - With no Basic ID, the page says "ยังไม่มีการตั้งค่า LINE ในอุปกรณ์นี้ จึงยังเพิ่มเพื่อนไม่ได้".
- **Fixed-recipient note on the page:** notifications go only to the LINE account configured on the device, and adding the OA as a friend does not change the recipient.
- **Unchanged:** label rename, LINE Test, disconnect and the maintenance arm/cancel flow. No token or recipient is ever rendered.

### 2. Unlock duration

**Server:** new `POST /api/manage/unlock-duration` (`WebServerManager::unlockDuration`).

- **Read:** `token` only, returns `{unlockSeconds, min:1, max:60, saved:false}`.
- **Update:** `token` + `seconds` (1–2 digits, 1–60). The handler:
  1. Checks the request shape with `validPost` first.
  2. Checks Owner authorization with `lineOwnerAuthorized()` (`managementAuthorized` plus `authorizedOwner`).
  3. Requires a healthy, configured `ConfigStore`.
  4. Requires the lock to be LOCKED with no active unlock timer; otherwise it returns 409 `unlock_active`.
  5. Calls `ConfigStore::save` with only `unlockDurationMs` changed. The existing A/B slot write, CRC and bounds validation still apply.
- It never calls `LockController::unlock` or `lock`. A change affects only future authorized or Emergency unlocks.
- Bounds are `kMinUnlockSeconds=1` and `kMaxUnlockSeconds=60`. They match the existing ConfigStore and Setup limits; no new range was invented.
- Errors: `invalid_duration` (400), `owner_required` (403), `unlock_active` (409), `storage_unavailable` (503).

**UI:** a number input (`inputmode=numeric`, min 1, max 60, step 1) showing the current value, with the unit "วินาที", a full-width "บันทึก" button and Thai feedback. Out-of-range or non-integer values are rejected before any request is sent.

### 3. Admin PIN change

**UI:** a dedicated page with three password fields, all `inputmode=numeric`, `pattern=[0-9]{4}`:
- "รหัส Admin PIN ปัจจุบัน"
- "รหัสใหม่ (ตัวเลข 4 หลัก)"
- "ยืนยันรหัสใหม่"

The form is reset immediately on submit, so the fields are empty after both success and failure. The JS copy is also cleared. Nothing is written to the URL, cookies or browser storage.

**Client-side checks (no request is sent):**
- Current PIN not 4 digits: "รหัส Admin PIN ปัจจุบันไม่ถูกต้อง"
- New PIN not 4 digits: "รหัสใหม่ต้องเป็นตัวเลข 4 หลัก"
- Confirmation differs: "รหัสใหม่ไม่ตรงกัน"

**Server changes (`adminPinChange`):**
- Still uses `ownerPinAuthorized()`: Owner only, lock re-asserted, locked and not unlocking, and the existing 3-per-minute web rate limit.
- New-PIN format and match are checked **before** the current PIN is verified, so those mistakes never use up a PIN attempt.
- A wrong current PIN still goes through `AdminPin::change` → `AdminPin::verify`. That means the shared durable failure counter, the 3-strike 60-second lockout and the LINE `ADMIN_PIN_FAILED` callback all apply unchanged.
- Distinct error codes let the page show clear Thai messages: `invalid_new_pin`, `pin_mismatch`, `current_pin_rejected` (403), `pin_locked` (429 with `Retry-After`), `storage_unavailable` (503). Success returns `{changed:true}`, shows "เปลี่ยนรหัส Admin PIN สำเร็จ" and still clears any armed LINE maintenance window.
- **1234 policy:** the current production source has no rule forbidding 1234 as a permanent PIN, at Setup or on change; `setup_browser_test.js` explicitly asserts "1234 accepted". Per the brief, that policy is preserved unchanged. Adding a rule is a possible later product decision.

## Authorization summary

| Function | Gate |
|---|---|
| LINE page | Owner (`lineOwnerAuthorized`) |
| Unlock duration read/update | Owner (`lineOwnerAuthorized`); update also requires LOCKED |
| Admin PIN change | Owner + LOCKED + web rate limit (`ownerPinAuthorized`) + current PIN (`AdminPin::verify`) |

No new role was added, and the Guest/User/Admin roles gain nothing.

## Preserved behaviour (verified unchanged)

These files were not touched: `LockController`, `LineNotifications`, `FactoryResetController`, `main.cpp`, `AdminPin.cpp`, `ConfigStore`, `NetworkManager`, `CanonicalOrigin`, `platformio.ini`.

- **Factory Reset (from `523fd0b`):**
  - Still calls `LineNotifications::suspendForReset()`, not `disconnect()`.
  - `sl-line` is still not cleared, so the token, recipient, enabled state, Basic ID, label, timezone, version and slot survive.
  - Touch calibration (`lock-touch-v2`) is still preserved.
  - Owner, identity and auth (SD `/smartlock`), Admin PIN, STA, AP, config and install state are still cleared.
- **C5 fresh install:**
  - `sl-line` is still excluded from `noPriorNvs`, and all non-LINE guards remain.
  - A genuinely fresh install still creates the temporary PIN, and Setup still requires choosing a new PIN.
  - A configured install with a missing or corrupt PIN store still fails closed.
- **Unchanged infrastructure:** no heap threshold, task stack/priority, TLS, mDNS or network change.

## Tests

**New or updated, all PASS:**

| Test | What it covers |
|---|---|
| `tests/admin/admin_test.cpp` (116 checks, up from 93) | `ConfigStore` rejects 999 and 60001 ms, accepts 1000 and 60000, and persists across reload. `AdminPin::change` with a wrong current PIN counts attempts and locks out on the third. During lockout even the correct PIN is Locked. An invalid new PIN does not count. A successful change is not stored in plaintext, the new PIN verifies after reload and the old PIN does not. |
| `tests/admin/web_test.js` | Exact success/error Thai texts. Client-side format and mismatch errors send no request. Fields cleared. No new 1234 rule. Unlock handler: Owner gate before save, `validPost` before gate, LOCKED check before save, no unlock call, single-field `ConfigStore` write, POST-only route, bounds equal to `ConfigStore`. Navigation order and Owner-only pages. Add Friend URL. Numeric/password inputs. No PIN in storage or URL. Unlock form behaviour including invalid values 0, 61, 2.5 and non-numeric. |
| `tests/admin/line_management_browser_test.py` (Playwright, 390×844 mobile) | Visible navigation labels. LINE readiness and enabled text. Green Add Friend button at least 44 px tall with the correct href, and clicking it (popup aborted) sends **no** SmartLock request. Unlock read, invalid rejection and save to 12 s with no access or unlock API. PIN mismatch and format errors (no request), wrong current PIN, success, old PIN rejected afterwards, lockout message, fields cleared, no PIN in storage or URL. No horizontal overflow. Unconfigured state hides Add Friend with a Thai message. Admin role cannot see or open the LINE, unlock or PIN pages. Evidence now goes to the system temp directory, not the repo. |
| `tests/admin/cleanup_ui.test.js` | Its navigation assertion was already stale on the base (it expected the retired `logs` and `cloud` pages). It now expects the new navigation and passes, including the dead-control check. |

**Existing relevant suites, all PASS:**

| Suite | Result |
|---|---|
| `tests/line/factory_reset_line_preserve_test.py` | Pass |
| `tests/line/run_line_tests.py` | Pass (suspend/preserve/reboot scenario) |
| `tests/admin/run_admin_tests.py` | Pass (plus 10 router/gesture cases, including physical Emergency and Reset) |
| `tests/security/run_security_policy_tests.py` | 43 checks, plus GPIO22 LockController ownership and the AccessController-only unlock |
| `tests/identity/run_identity_tests.py` | 349 checks |
| `tests/registration/run_registration_tests.py` | Pass |
| `tests/network/run_network_manager_tests.py` | 155 checks |
| `tests/network/run_canonical_origin_tests.py` | 20 checks |
| `tests/http_deadline/run_http_deadline_tests.py` | Pass |
| `tests/touch/run_touch_state_tests.py` | Pass |
| `tests/admin/setup_browser_test.js` | Pass |

**Pre-existing stale failures (identical errors on the base; not regressions):**
- `tests/events/run_event_log_tests.py` and `tests/phone2/run_phone2_tests.py`: the retired `EventLog.h` is missing.
- `tests/run_defect_fix_regression.py`: the harness fails with ``'network' undeclared`` against the current `main.cpp`.

## Build (`pio run -e esp32_035`, build only)

| Item | Value |
|---|---|
| Result | SUCCESS |
| RAM | 112,612 / 327,680 bytes (34.4 %), unchanged |
| Flash | 1,239,785 / 1,310,720 bytes (94.6 %), +7,384 bytes vs `523fd0b` |
| `firmware.bin` | 1,246,368 bytes |
| SHA-256 | `d208be3596feda245e584dcb034ec210132565b79528d57c2a94e5c3bfdc621a` |

For comparison:
- The previous development build (`523fd0b`) was `bbbba22ab192a41301646cf0149486c0fa58e17eea400737deca77ae4da1cc7d`.
- The accepted production image is `084227e2e77994ae21b773e253c209b8ba49b897354b3969a5f76f328cb06be9`.

## Remaining real-board acceptance (PENDING)

1. App-only flash to `0x10000` (otadata selects app0), then `verify_flash`.
2. On the Owner phone:
   - The LINE page shows Ready/On and the green Add Friend button.
   - Change the unlock duration and confirm the next Owner Access uses the new time. The physical release and relock must be confirmed by the user.
   - Change the Admin PIN, confirm the new PIN works on the TFT Admin menu and the old PIN does not.
3. The Factory Reset LINE-preservation acceptance from `523fd0b` is still outstanding: Factory Reset, fresh Setup with no C5 systemFault, Wi-Fi, LINE Ready without reprovisioning, Add Friend visible, and LINE Test receipt confirmed by the user.
4. Earlier field observation: weak Wi-Fi (about −90 dBm) made `.local` and Management unreachable. Testing should be done with adequate signal.
