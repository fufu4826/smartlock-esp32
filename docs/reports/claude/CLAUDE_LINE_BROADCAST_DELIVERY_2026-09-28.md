# LINE delivery changed to broadcast to all friends of the Official Account (2026-09-28)

Status: **post-acceptance prototype development.** Source, tests and build pass. **Real-board broadcast acceptance is PENDING.**

| Item | Value |
|---|---|
| Base commit | `a5325b5` |
| New commit | `eaac772` "feat: broadcast line notifications to all friends" |
| Old accepted tag | `smartlock-final-accepted-2026-09-28` still points to `d1e5ce3` (unchanged) |
| New acceptance tag | not created |

No flash, reset or board change was made in this package.

## Old vs new

| | Old (up to `a5325b5`) | New (`eaac772`) |
|---|---|---|
| Endpoint | `POST /v2/bot/message/push` | `POST /v2/bot/message/broadcast` |
| Body | `{"to":"<fixed User ID>","messages":[…]}` | `{"messages":[…]}` with no `to` |
| Who receives | One LINE account whose User ID was stored on the device | Every friend of the single LINE Official Account |
| Config required for Ready | Channel Access Token **and** recipient User ID | Channel Access Token (plus enabled flag and valid v2 metadata) |
| Joining notifications | Admin had to provision that account's User ID | The person presses **เพิ่มเพื่อน LINE** and adds the OA |

## Why recipient binding was removed

**What happened in the field.** The real-board run of `a5325b5` showed the problem:
- Factory Reset correctly preserved LINE.
- Notifications kept going to the **old** fixed recipient, not to the phone of the new Owner.
- LINE does not reveal a friend's User ID to the device unless a webhook receives the follow event. That requires a public HTTPS server, which the zero-cost, no-cloud requirement rules out.

**The new product rule.** Notifications go to everyone who is a friend of the SmartLock OA.
- Anyone who adds the OA receives them. This is **intentional**: there is no per-friend authorization.
- Message text includes the device label, event, time and, for unlocks, the SmartLock identity ID, name, role and source.
- People who should not see this should not be given the OA, and the Basic ID page is shown only to the Owner.

## Architecture (zero-cloud)

```
ESP32 --TLS (pinned root CA)--> api.line.me /v2/bot/message/broadcast --> all friends of the OA
```

- **Unchanged transport:** the direct TLS worker, pinned CA, Bearer token header, `X-Line-Retry-Key` idempotency, 12 s request deadline, heap admission (≥90,000 free / ≥40,000 largest block), RAM queue of 16, retries and TTL.
- **Nothing external:** no webhook, server, Cloudflare Worker, database or billing-enabled service.
- **Scope of LINE failures:** a LINE failure (Offline, AuthRequired, QuotaFull, ServiceError) only stops notifications. `AccessController`, the physical Emergency path and the lock timer never wait on LINE.

## Events

All kinds share the same `buildDeliveryBody()` and `kDeliveryPath`:
- `UNLOCK_SUCCESS`
- `ADMIN_PIN_FAILED` (including lockout)
- `ADMIN_EMERGENCY_UNLOCK`
- the Management LINE Test ("ส่งข้อความทดสอบถึงเพื่อนทุกคน")

No push path remains in the source.

## Persistent config and migration decision

- **Layout kept as-is:** Config **v2** in NVS `sl-line` (A/B slots `cfg0`/`cfg1`, `active` selector).
- **No v3 and no migration write.** Existing deployed slots load unchanged; the host test checks that NVS is byte-identical after load.
- **The `userId` field is deprecated and ignored:**
  - it stays in the struct only to keep the binary layout
  - it is never sent or rendered
  - it is optional during validation (empty is allowed; a malformed non-empty value still invalidates the slot, as before)
  - `save()` writes it empty
- **Preserved without any rewrite:** the Channel Access Token, Basic ID, enabled flag, label, timezone and version.
- **Migration from v1** (legacy `config` key) no longer requires a recipient.
- **Maintenance commit** (`/api/line/maintenance/commit`) needs only `token`, `nonce`, `label`, `lineToken` and `publicBasicId`. A deprecated `userId` from older tooling is accepted as a sixth field and ignored; any other extra field is still rejected.
- **Rollback note:** existing configs keep their old recipient bytes, so rolling back to `d1e5ce3` or `a5325b5` still works with them. A config **newly saved** by `eaac772` has no recipient; older push firmware would treat it as not configured.

## Factory Reset

The policy is unchanged from `523fd0b`:
- `performReset()` calls `LineNotifications::suspendForReset()`, a runtime-only stop, and never clears `sl-line`.
- The token, enabled state, Basic ID, label, timezone and version all survive. Touch calibration survives too.
- The Owner, identities and auth (SD `/smartlock`), Admin PIN, STA/AP, config and install state are cleared.
- The C5 fresh-install check still excludes `sl-line`.

After a reset, broadcasts resume as soon as Wi-Fi and time are available. The recipient no longer matters: any friend of the OA, old or new, receives them.

## Add Friend

This is unchanged: a plain `https://line.me/R/ti/p/<publicBasicId>` link that opens in a new tab.
- It writes nothing, binds no one and has no JS handler.
- The page text now says: "การแจ้งเตือนจะถูกส่งไปยังเพื่อนทุกคนของบัญชี LINE Official Account นี้ กดเพิ่มเพื่อน LINE เพื่อรับการแจ้งเตือนจาก SmartLock".

## Quota

- **How LINE counts it:** a broadcast counts against the OA's monthly quota **per recipient**. For example, 1 event to 5 friends counts as about 5 messages.
- **What the firmware tracks:** it already reads the official read-only `GET /v2/bot/message/quota` and `/quota/consumption`, shows used / limit on the LINE page, and stops sending when used ≥ limit (QuotaFull). The local +1 reservation per send is only a lower bound for broadcasts.
- **What changed:** after any accepted broadcast, the official consumption is re-read after about 5 s.
- **Unchanged:**
  - there is no paid fallback or billing behaviour
  - friend count is never inferred
  - exhaustion just makes LINE delivery fail (it shows in diagnostics as QuotaFull or failed counters)
  - door operation is unaffected

## Security (unchanged)

- The token lives only in device NVS and is never logged, rendered, put in a URL or stored in the browser.
- TLS verification with the pinned root is unchanged.
- LINE settings, unlock duration and PIN change remain Owner-only.
- GPIO22 and `LockController` ownership, Admin PIN and Emergency behaviour are untouched.

## Tests

**PASS:**

| Suite | What it covers |
|---|---|
| `tests/line/run_line_tests.py` | Endpoint constant is `/v2/bot/message/broadcast`. The real request line is `POST /v2/bot/message/broadcast` with no `/push` and no `"to"`. Unlock, PinFailure, Emergency and Test bodies all start `{"messages":[…` with no `to` and no User ID. A saved config has an empty recipient and is still enabled and configured. A deployed v2 config **with** a recipient loads as configured/enabled with the token and Basic ID intact and NVS byte-identical, and survives `suspendForReset`. A malformed recipient still rejects the slot; an empty recipient with a token is valid; enabled with no token is invalid. The suspend, preserve and reboot scenario passes. |
| `tests/admin/line_maintenance_test.py` | The recipient-free commit saves the token and Basic ID. A legacy `userId` is accepted and ignored. Unknown extra fields are rejected. Replay, size bounds and the PIN-gated arm still hold. |
| `tests/line/factory_reset_line_preserve_test.py` | Reset clear-list, `suspendForReset`, C5 guards, broadcast path, recipient-free validation and save signature, Add Friend link only. |
| `tests/admin/web_test.js` | New commit envelope; no `copyArg("userId")`; no `message/push` or `"to"` in the LINE source; new Thai wording present; old fixed-recipient wording absent; plus all earlier unlock-duration and PIN checks. |
| `tests/admin/line_management_browser_test.py` | Mobile Playwright, 8 checks. |
| Other suites | `tests/admin/run_admin_tests.py` (116 checks plus 10 router cases), security (43 checks plus GPIO22 ownership), identity (349), registration, network (155 + 20), HTTP deadline, touch, setup browser, cleanup UI |

**Pre-existing stale failures (unrelated, unchanged):** the event-log and phone2 suites (retired `EventLog.h`) and the defect-fix regression harness (``'network' undeclared``).

## Build (`pio run -e esp32_035`, build only)

| Item | Value |
|---|---|
| Result | SUCCESS |
| RAM | 112,612 B (34.4 %) |
| Flash | 1,239,813 / 1,310,720 B (94.6 %) |
| `firmware.bin` | 1,246,384 B (app0 headroom 64,336 B) |
| SHA-256 | `546e96b03583eaf5405bbd7b1a6100cfee91937ba9047cc0026db4f4caa58e36` |

For comparison: `a5325b5` was `d208be35…621a`, and the accepted `d1e5ce3` image is `084227e2…6be9`.

## Context: `a5325b5` real-board run (not tagged; superseded)

The following were observed on the board (USER-PHYSICAL-CONFIRMATION plus serial):
- **Flash:** app-only flash to `0x10000` with `verify_flash` OK.
- **Management UI:** 3 pages. The LINE page showed Ready/On with Add Friend, and a LINE Test arrived.
- **Unlock duration:** set to 10 s; the lock physically released for about 10 s, relocked, and UNLOCK_SUCCESS arrived.
- **Admin PIN change:** the web change succeeded and the new PIN opened the TFT Admin menu. The old-PIN rejection was not registered by the counters.
- **Factory Reset:** GPIO22 LOCKED at boot, and first boot showed `Configured: NO` and `Owner exists: NO`.
- **Fresh Setup:** the verifier was absent, then `ADMIN PIN STORE: OK` (C5 PASS). Calibration loaded, `STA saved config: NONE`, and Setup and Wi-Fi completed.
- **LINE after reset:** configured/enabled/v2, then Ready after a transient TLS error right after STA came up.
- **The limitation that drove this change:** notifications reached only the old fixed recipient.
- **Signal:** earlier `.local` failures were caused by about −90 dBm Wi-Fi; runs near the router saw −69 to −80 dBm.

## Pending real-board acceptance for `eaac772`

1. App-only flash to `0x10000`, then `verify_flash`.
2. Without reprovisioning: LINE Ready, and the LINE Test arrives on **every** friend's phone (old and new).
3. The new Owner's phone, after Add Friend, receives UNLOCK_SUCCESS on one Access unlock (physical release and relock confirmed by the user).
4. Optionally, `ADMIN_PIN_FAILED` from one wrong PIN.
5. Check the quota shown on the LINE page after a broadcast.
