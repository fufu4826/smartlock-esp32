# Factory Reset preserves prototype LINE configuration — implementation, 2026-09-28

Status: **post-acceptance prototype change. Real-board Factory Reset acceptance is PENDING.** This change is not final-accepted. Tag `smartlock-final-accepted-2026-09-28` is unchanged and still peels to `d1e5ce38058790a973ce90c1addf33ef206ce4cd`. No flash, reset or board access was performed.

## Preconditions

- **Rollback backup verified independently before any source edit.**
  - Location: `D:\SmartLock_Backups\SmartLock_FINAL_ACCEPTED_2026-09-28_d1e5ce3`.
  - `release/firmware_ACCEPTED.bin` SHA-256 is `084227e2…cbe06be9`, which is the accepted image.
  - `git bundle verify` passes with complete history. The bundle heads are master = `d1e5ce3` and the tag, which peels to `d1e5ce3`.
  - `project_full` HEAD and the tag both resolve to `d1e5ce3`, and `fsck` reports no errors.
  - Zip CRC test: no bad entries (3,734 files).
- **Base commit.** Work is based on `570f382` ("chore: archive post-acceptance SmartLock materials"). The user made that commit after the earlier audit. It moved `docs/`, `tools/` and similar folders to `D:\SmartLock_Documentation_2026-09-28`.
- **Earlier audit report lost.** `CLAUDE_FACTORY_RESET_LINE_PERSISTENCE_AUDIT_2026-09-28.md` was untracked when that commit removed `docs/`, and no copy exists in either D: archive. Its findings are summarised below.

## Previous behaviour (accepted `d1e5ce3`)

`FactoryResetController::performReset` ran these steps in order:

1. `lock_.lock()`
2. `LineNotifications::disconnect()`
3. Verified GPIO22 is locked
4. Invalidated sessions
5. `WiFi.eraseAP()`
6. Deleted SD `/smartlock`
7. Cleared these NVS namespaces: `sl-config`, `sl-net`, `sl-install`, **`sl-line`**, `sl-pin`, `sl-generation`, `sl-ota`, `sl-sta`, `sl-cloud`, `sl-audit`, `p2-test`
8. Scheduled a restart

**Disconnect issue (FOUND).** `disconnect()` is a persistent erase, not just a runtime stop. It writes a new active config slot with `enabled=0`, an empty token and an empty recipient. It keeps the label, timezone and Basic ID. It then removes the other slot and the legacy `config` key. So dropping `clearNamespace("sl-line")` on its own would still have destroyed the token and recipient.

Touch calibration (`lock-touch-v2`) was already preserved.

## New requirement

Factory Reset must preserve the complete, valid LINE configuration: the token, the fixed recipient User ID, the enabled state, the OA Basic ID, the label, the timezone, the version and the active-slot selector. After reset, fresh Setup and Wi-Fi, LINE should become Ready with no reprovisioning.

## Source changes

1. **`src/notifications/LineNotifications.{h,cpp}` — new `suspendForReset()`.** This is a runtime-only stop:
   - sets a RAM `gSuspended` flag
   - under the config mutex, clears `gEnabled`/`gConfigured`, sets state Disabled, zeroes the RAM config cache (token/recipient) and bumps `gConfigGeneration`, so an in-flight worker loop abandons after its current request and cannot start another
   - clears the RAM queue and wakes the worker
   - performs no `Preferences` access and no `writeConfig`

   While suspended:
   - `save`, `rename` and `disconnect` return `StorageError` without writing
   - `refreshConfigState` is a no-op, so nothing can re-enable delivery before reboot
   - emitters and `requestTest` are already gated by `gEnabled`

   The flag is RAM only, so reboot clears it.
2. **`src/app/FactoryResetController.cpp`:**
   - `performReset` calls `suspendForReset()` instead of `disconnect()`.
   - `clearNamespace("sl-line")` is removed.
   - A comment documents that `sl-line` and `lock-touch-v2` are preserved on purpose.
   - Ordering is unchanged: lock, then LINE stop, then the GPIO22 locked check, then erase.
3. **`src/main.cpp` — C5 fresh-install check.** `noPriorNvs` previously required the `sl-line` keys `active`, `cfg0`, `cfg1` and `config` to be absent. With `sl-line` preserved, the first boot after a reset would have failed that check. `AdminPin::begin` would then refuse to create the initial PIN, and the device would enter `systemFault` with **no way to run Setup**. The `sl-line` keys are therefore removed from `noPriorNvs`, and a comment explains why. The remaining guards are unchanged:
   - `ConfigStore` `CreatedDefaults`
   - `sl-install` `id`
   - `sl-pin` `initial` and `fail`
   - `sl-net` `ap-pass`
   - `sl-sta` `cfg-a` and `cfg-b`
   - identity mode
   - AuthStore Missing
   - absence of the SD user/device/auth record, backup and temp files

   `sl-line` holds no identity, PIN or auth authority, so leaving it out does not let a configured installation mint a default PIN.
4. **`README.md`:** notes the prototype reset policy and the fixed-recipient limitation.
5. **Tests:** `tests/line/line_notifications_host.cpp` gains a reset scenario, and there is a new `tests/line/factory_reset_line_preserve_test.py` source-contract check.

No other reset target, lock logic, Admin PIN, Owner auth, web route or Add Friend code changed.

## Reset matrix after this change

| Store | Factory Reset |
|---|---|
| SD `/smartlock` (Owner, identities, auth verifiers, inert history) | DELETED |
| `sl-pin` (Admin PIN) | CLEARED; the temporary default is recreated only on a genuinely fresh install |
| `sl-sta` + SDK STA credentials (`WiFi.eraseAP`) | CLEARED |
| `sl-net` (protected AP) | CLEARED and regenerated |
| `sl-config`, `sl-install`, `sl-generation` | CLEARED and recreated |
| `sl-ota`, `sl-cloud`, `sl-audit`, `p2-test` | CLEARED |
| `lock-touch-v2` (calibration) | PRESERVED (unchanged) |
| **`sl-line`** | **PRESERVED (new)** |

## Preserved LINE fields

Everything in `sl-line` is kept untouched, byte for byte:

- `active` slot selector plus the active `cfg0`/`cfg1` (`Config` v2), which contains:
  - `version`, `enabled`, `timezoneMinutes`
  - `label`
  - Channel Access Token
  - recipient User ID
  - `publicBasicId`
- any inactive slot or legacy `config` key, if present

At boot, `refreshConfigState` validates and loads the config as it normally would.

## Security trade-off and fixed-recipient limitation

- Factory Reset **no longer erases LINE secrets** on this prototype. A person who performs a physical Admin reset, or who later owns the device, keeps a device that pushes to the original recipient using the original token. The user explicitly accepts this.
- The token remains only in device NVS. It is never shown in the UI, a URL or browser storage, and no code path logs it.
- The recipient is **fixed**. Notifications always go to the stored User ID.
  - Add Friend still only opens `https://line.me/R/ti/p/<publicBasicId>`.
  - It does not discover or bind a new User ID and writes nothing to the device.
  - Another LINE account adding the OA does not receive notifications.
  - There is no webhook or follower discovery.
- To erase LINE from the device on purpose, use the Owner Management LINE Disconnect (`/api/line/disconnect`) **before** resetting.

## Expected post-reset flow (to be proven on the board)

1. Factory Reset, then reboot.
2. Fresh Setup: Owner name, unlock duration, new Admin PIN.
3. Reconnect home Wi-Fi.
4. LINE reports Offline or TimeUnavailable until STA and time are available, then Ready from the preserved config.
5. The Add Friend button appears at once, because the Basic ID is preserved. Press it only if needed.
6. Run LINE Test and confirm receipt.

LINE state never gates door authorization or relock.

## Tests (host; no board)

**PASS:**

| Suite | Result |
|---|---|
| `tests/line/run_line_tests.py` | Existing cases plus the new reset scenario (details below) |
| `tests/line/factory_reset_line_preserve_test.py` | Contract check (details below) |
| `tests/admin/run_admin_tests.py` | 93 focused checks and 10 router/gesture cases, including C5 `AdminPin::begin` fresh/configured/corrupt fail-closed and physical Emergency/Reset routes |
| `tests/security/run_security_policy_tests.py` | 43 checks, plus GPIO22 LockController ownership and the AccessController-only unlock |
| `tests/identity/run_identity_tests.py` | 349 checks |
| `tests/registration/run_registration_tests.py` | Pass |
| `tests/network/run_network_manager_tests.py` | 155 checks |
| `tests/network/run_canonical_origin_tests.py` | 20 checks |
| `tests/http_deadline/run_http_deadline_tests.py` | 11 checks |
| `tests/touch/run_touch_state_tests.py` | Pass |

- **Reset scenario in `run_line_tests.py`:**
  1. Save a configuration and queue an event.
  2. Call `suspendForReset`. Status shows not configured, not enabled, queue 0 and Disabled. The RAM token is zeroed, the worker snapshot is refused, and NVS is **byte-identical**.
  3. Emitters, test, save, rename, disconnect and refresh are all blocked, and NVS is still identical.
  4. Simulated reboot: the configuration loads as configured and enabled, version 2, with the same label and Basic ID. The token, recipient, timezone and active slot are intact.
- **`factory_reset_line_preserve_test.py` asserts:**
  - the exact cleared-namespace list, with no `sl-line` and no `lock-touch-v2`
  - SD tree deletion and `eraseAP` are still present
  - `suspendForReset` is used and `disconnect` is not
  - order: lock, then LINE stop, then GPIO22 check, then erase
  - `noPriorNvs` keeps every non-LINE guard and has no `sl-line`
  - C5 `AdminPin` guards are present
  - `suspendForReset` has no persistent writes
  - suspended guards exist in `save`, `rename` and `disconnect`
  - Add Friend is only a `line.me` href with no click handler and no friend route

Fixtures use only mock tokens and IDs.

**Pre-existing failures (identical on unmodified `570f382`; not caused by this change):**

- `tests/events/run_event_log_tests.py` and `tests/phone2/run_phone2_tests.py`: the retired `src/events/EventLog.*` no longer exists.
- `tests/run_defect_fix_regression.py`: its harness no longer matches `main.cpp`'s `renderCurrentState`, failing with ``'network' undeclared``.

Physical C6 Emergency network independence is covered by the passing admin router tests and the unchanged `main.cpp` Emergency path. There is no separate C6 host suite in this checkout.

## Build (build only)

`pio run -e esp32_035`: **SUCCESS**

| Item | Value |
|---|---|
| RAM | 112,612 / 327,680 bytes (34.4 %) |
| Flash | 1,232,401 / 1,310,720 bytes (94.0 %) |
| `firmware.bin` | 1,238,976 bytes |
| Development SHA-256 | `bbbba22ab192a41301646cf0149486c0fa58e17eea400737deca77ae4da1cc7d` |

This SHA is a development artifact, **not** the accepted deployed firmware (`084227e2e77994ae21b773e253c209b8ba49b897354b3969a5f76f328cb06be9`).

## Remaining acceptance (PENDING, requires the board)

1. Flash the development build by USB. The currently provisioned LINE config must already exist on the device.
2. Perform a physical Admin Factory Reset.
3. Confirm Setup is offered and the temporary PIN is created. This proves the C5 check change works on hardware.
4. Complete Setup and Wi-Fi.
5. Confirm `DIAG_SYSTEM` shows LINE configured/enabled and then Ready.
6. Confirm Add Friend is visible.
7. Run LINE Test and confirm receipt on the fixed recipient's phone.
8. Confirm GPIO22 stays locked throughout reset and reboot.
