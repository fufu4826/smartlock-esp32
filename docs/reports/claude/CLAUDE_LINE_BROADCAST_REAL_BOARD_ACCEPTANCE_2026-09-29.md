# LINE broadcast: real-board acceptance record (`eaac772`)

Dates: 2026-09-28/29. Author: Claude (Claude Code). No secret values appear in this record.

## Result

- **Summary:** software evidence PASS; the user declared the work complete.
- **Not recorded individually:** the per-phone receipt answers requested in the acceptance package (Phone A / Phone B, for the test and for UNLOCK_SUCCESS). The user's final message was "โอเคงานนี้เสร็จเรียบร้อยทั้งหมดแล้ว" (all work is complete).
- **Tag:** `smartlock-line-broadcast-accepted-2026-09-28` was **not** created. It needs explicit per-item confirmation.
- **Old tag:** `smartlock-final-accepted-2026-09-28` still points to `d1e5ce3` (unchanged).

## Evidence

| Item | Result | Evidence class |
|---|---|---|
| Source | `eaac772` (clean tree) | SOURCE |
| Firmware | SHA-256 `546e96b03583eaf5405bbd7b1a6100cfee91937ba9047cc0026db4f4caa58e36`, 1,246,384 B | SOFTWARE |
| Board | CH340 1A86:7523 on COM6; ESP32-D0WD-V3 rev 3.1, MAC `…5a:22:04` | DEVICE |
| Application-only flash | `write_flash 0x10000` only (bootloader, partitions, NVS and SD untouched) | DEPLOYMENT |
| `verify_flash` | digest matched | DEPLOYMENT |
| Boot after flash | GPIO22 LOCKED, Owner present, `unlock_ms=10000`, calibration and PIN store OK, STA 192.168.1.179 (about −77 dBm) | SERIAL DIAG |
| Existing Config v2 (with old recipient bytes) | loaded without any write or reprovisioning: `configured=1 enabled=1 config_version=2`, state **Ready** | SERIAL DIAG |
| Recipient required | No (Ready with the recipient ignored) | SERIAL DIAG + source |
| Real events broadcast | 1 × UNLOCK_SUCCESS and 2 × ADMIN_PIN_FAILED: `push_started=3 push_written=3 failed=0`, last HTTP **200** | SERIAL DIAG |
| Endpoint | The firmware has a single delivery path, `/v2/bot/message/broadcast`, with no `to` field | SOURCE + host tests |
| Owner Access | `ACCESS: UNLOCKED`, then `LOCK STATE: LOCKED (hardware timer)`; finished LOCKED | SERIAL |
| LINE Test button | not observed in the captured serial window (`test_state=0` at the last read) | SERIAL DIAG |
| Per-phone receipt (A/B) | not individually stated; general "all complete" statement | USER STATEMENT |
| Factory Reset on `eaac772` | NOT RE-RUN; inherited from the `a5325b5` run on the same lineage (reset list and C5 unchanged) | INHERITED |
| GPIO22 at the end | LOCKED at the last reading; the board was then unplugged | SERIAL DIAG |

## Quota note

Broadcasts count once per friend against the OA's monthly quota. The LINE page shows official quota and usage when available. Running out of quota stops only notifications, never unlocking or relocking.

## To finalize (optional)

1. Confirm that the test message and the "ปลดล็อกสำเร็จ" message each arrived on **both** LINE accounts.
2. Then create the annotated tag `smartlock-line-broadcast-accepted-2026-09-28` on `eaac772`.

---

## Final status update (2026-09-29): development closed

The earlier sections above are kept unchanged as the record at the time. The user then gave the final product decision.

**Tag:** `smartlock-line-broadcast-accepted-2026-09-28` was created on **`eaac772`** (annotated). `smartlock-final-accepted-2026-09-28` still points to `d1e5ce3`. `cd4c451` is documentation only.

**PASS (user-confirmed on hardware unless noted):**
- Normal Owner Access
- Timed relock
- Configurable unlock duration (10 s observed)
- Management Admin PIN change
- Factory Reset (on the `a5325b5` lineage; not re-run on `eaac772`)
- LINE configuration preserved across reset
- Fresh Setup after reset (C5 PASS)
- LINE broadcast architecture
- Multiple OA-friend recipients
- Add Friend flow
- LINE Test and normal broadcast notifications confirmed by the user on the tested friend devices
- GPIO22 safety (LOCKED at every reading)

**KNOWN ACCEPTED LIMITATION (not PASS):**
- The physical TFT Admin **Emergency Unlock** releases the lock correctly, but its `ADMIN_EMERGENCY_UNLOCK` LINE notification is **not received**.
- Host tests show the event uses the same broadcast body and path. On hardware the notification is absent.
- The cause was not investigated, by user decision.

**Decision:** the user accepted the prototype in this state and requested no further firmware changes. Final development status: **CLOSED**.
