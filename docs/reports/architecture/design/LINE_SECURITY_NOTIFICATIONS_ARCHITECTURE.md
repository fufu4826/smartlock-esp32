# LINE Security Notifications — L0 Architecture

Status: L0 design. Implementation supersession: the subsequent user-approved LINE implementation explicitly replaces USB-only provisioning with Owner-only POST configuration in existing HTTP Management. This accepts the LAN transport tradeoff; token password input, no token readback/logging, and existing session/CSRF authorization remain mandatory. No event data is persisted. See LINE_NOTIFICATIONS_IMPLEMENTATION_REPORT.md for current implementation evidence.
Date checked: 2026-09-27 (Asia/Bangkok). Architect: ASTRA.

## 1. Executive summary

Select **direct ESP32 → HTTPS LINE Messaging API Push → one Owner's LINE**. Use a dedicated Thailand Free Official Account (OA), one long-lived channel access token and one recipient. No relay, webhook, cloud database, Google integration or inbound commands.

Each successful local unlock, each completed incorrect Admin PIN comparison, and each successful physical Emergency Unlock produces one RAM notification. Local authorization and GPIO operation finish before enqueueing. Delivery is best effort: finite quota, queue capacity, outages, expiry, reboot and recipient blocking prevent a guarantee of delivery for every event. Every qualifying event is offered to the queue; no events are silently coalesced.

Future firmware stops writing operational event history to SD, while retaining the SD-backed authorization database. This document does not change any code or stored data. LINE chats themselves retain notifications according to LINE/user behavior; “no history” here means no SmartLock operational event store or external event database, not disappearing LINE messages.

Authority: `docs/phase_reports/GOOGLE_SHEETS_REMOVAL_REPORT.md`. Cleanup commit `b6db664e6326eb8d62c45a8317beef6eabf8584c`; current HEAD `07ef632` adds report bookkeeping. Google archive `4439bdbab8ab0260175891b0e45c6e079613f8ce`, tag `google-sheets-archive-before-removal`. Source inspection, not old cloud reports, informs this design. Installed COM6 firmware is a separate, older baseline; it was not inspected or changed for L0.

## 2. User requirements

- Preserve local Owner/identity/credential/role, Access, Management, PIN, physical Admin, Wi-Fi, canonical hostname, touch and unlock-duration rules.
- GPIO22 HIGH = energized/LOCKED; LOW = released/UNLOCKED. Boot remains LOCKED. Only LockController writes the pin.
- Notification failure never denies otherwise valid Access, extends unlock, changes authorization, triggers reset, or changes GPIO.
- Zero paid services, upgrades, overages, billing setup, domains or fallback providers.
- One concise Thai notification per required security event; no routine relock, boot or network notifications by default.
- RAM-only pending delivery and loss counters. No SD/NVS event journal, payload backup, retry journal or historical Management view.
- Persistent notification **configuration** is permitted; it is not an event queue.

## 3. Official LINE service research

All rows checked 2026-09-27. These are current documentation facts, not proof that a particular account has already been configured.

| Fact | Official source |
|---|---|
| LINE Notify ended on 2025-03-31; do not use it. | [LINE end-of-service announcement](https://developers.line.biz/en/news/2024/?article=line-notify-will-be-discontinued&day=07&month=10) |
| Thailand Free OA lists 300 messages/month, no package charge or additional-message option. | [Thailand plan table](https://lineforbusiness.com/th/service/line-oa-features), [Thailand OA help](https://help2.line.me/official_account_th/?contentId=20011832) |
| Push is counted by recipients, not message-object count. Excess monthly quota produces an error and no send. | [Messaging API pricing](https://developers.line.biz/en/docs/messaging-api/pricing/) |
| Messaging API remains available; create an OA, then enable Messaging API in OA Manager. Direct channel creation in Developers Console is discontinued. | [Getting started](https://developers.line.biz/en/docs/messaging-api/getting-started/) |
| Long-lived: indefinite, one/channel; short-lived: 30 days; v2.1: up to 30 days; stateless: 15 minutes and not revocable. | [Channel token types](https://developers.line.biz/en/docs/basics/channel-access-token/) |
| Own recipient ID is available as “Your user ID” in channel Basic settings when the developer Business ID is linked to LINE. IDs are provider-scoped, not phone numbers or public LINE IDs. | [Getting user IDs](https://developers.line.biz/en/docs/messaging-api/getting-user-ids/) |
| Push endpoint is `POST https://api.line.me/v2/bot/message/push`. Up to five objects/request; ordinary text maximum 5,000 characters. Push rate limit is 2,000 requests/second/channel. Quota APIs expose limit and approximate consumption. | [API reference](https://developers.line.biz/en/reference/messaging-api/nojs/) |
| Push eligibility includes OA friends; blocking/deletion can prevent delivery despite HTTP 200. Do not use the temporary non-friend interaction allowance as permanent eligibility. | [Push reference](https://developers.line.biz/en/reference/messaging-api/#send-push-message) |
| UUID retry keys must be used from the initial request. Accepted keys persist for 24 hours; retrying yields 409 with accepted-request information. Acceptance does not guarantee delivery. | [Retrying requests](https://developers.line.biz/en/docs/messaging-api/retrying-api-request/) |
| API hosts require TLS 1.2 or later; webhook-server certificate advice is not a trust-root specification for outbound API calls. | [LINE TLS announcements](https://developers.line.biz/en/news/2021/) |

Read-only live TLS observation from the development PC, without token or HTTP push: a validated TLS 1.2 connection to `api.line.me` succeeded. Observed chain: `api.line.me` → `DigiCert Global G3 TLS ECC SHA384 2020 CA1` → `DigiCert Global Root G3`. TLS 1.3 also negotiated separately. This is a dated observation, not a promise about all future edges or certificates. No account, channel, recipient or token was created.

## 4. Zero-cost feasibility

**Yes, under the current Thailand Free plan, with finite best-effort delivery.** The dedicated account must remain Free with no add-ons or payment setup. Firmware never calls a purchasing/upgrade API. At the limit it stops pushing; no paid fallback exists.

300 is the OA's monthly budget, shared with other counted sends, including tests and OA Manager activity. Ten notifications/day approximately consumes the entire monthly allowance; wrong-PIN attacks can exhaust it. It is impossible to promise unlimited “every event delivered” and simultaneously retain the Free cap. Required event generation is unconditional; successful delivery is conditional on quota and service availability. Existing Wi-Fi/Internet is assumed, not a new purchased service.

## 5. Candidate architectures

| Option | Benefit | Cost/maintenance/security | Decision |
|---|---|---|---|
| A. Direct ESP32 → Messaging API | No hosting or database; one destination; few request types | Device holds channel bearer token; TLS and retries use ESP resources | SELECT |
| B. ESP32 → free relay → LINE | Token can live off-device | Another credential, service, deployment and quota; free-tier availability risk; no benefit needed here | Reject |
| C. Other LINE-native path | Replies/LIFF/LINE Login can support interactive applications | Replies depend on user interaction; Login is not an unattended notification sender; Notify is gone | No simpler replacement |

Do not resurrect Google infrastructure to relay LINE. No SDK, JWT framework, LIFF site or webhook is needed for this one developer/Owner recipient.

## 6. Selected architecture

Dedicated OA owned by the intended recipient. Store its long-lived token and provider-correct user ID in a separate versioned NVS notification configuration. Provision over a trusted USB connection, not the existing HTTP Management page. Daily controls remain Owner-only in Management.

Use Arduino-ESP32's existing TLS facilities and a small bounded JSON encoder/parser. A low-priority worker owns network I/O. Queue mutation copies small records under a short critical section; it never holds authorization, SD or lock-related mutexes during network operations. The worker cannot reference LockController mutators.

## 7. Component diagram

```text
Existing local authorization / PIN verifier / physical Admin
             | local result already decided; unlock already applied
             v
 SecurityEventEmitter (bounded copies, no I/O, no SD dependency)
             |
             v
 LineNotificationQueue (16 fixed RAM slots)
             |
             v
 Low-priority LineClient worker ---- NVS notification config (token/recipient)
             | HTTPS, fixed api.line.me host, CA + hostname verification
             v
 LINE Messaging API -----> Owner's LINE chat

No arrow returns to authorization, GPIO, PIN, reset or configuration.
Management reads RAM status; authorized controls set notification-only commands.
SD continues serving identities/authentication; it is not a queue or history sink.
```

## 8. Notification event model

Three production security types only:

| Type | Exact trigger | Snapshot / exclusions |
|---|---|---|
| `UNLOCK_SUCCESS` | `AccessController::request` gets `UnlockResult::Unlocked` | Existing verified `IdentityRecord`: ID/name/role, actual LAN or Recovery AP source, Access QR method. AlreadyUnlocked/replay/denied requests do not qualify. |
| `ADMIN_PIN_FAILED` | A valid-format submitted PIN completes hash comparison and differs | Wrong count, lockout-started flag, source, logical lock-state snapshot. Not cancel/delete/partial input/malformed input or an attempt rejected during lockout. |
| `ADMIN_EMERGENCY_UNLOCK` | Existing confirmed physical Admin path actually unlocks | Physical Admin source, unknown individual operator. No invented Owner identity. This replaces, rather than accompanies, generic unlock notification. |

`AdminPin::verify` currently returns `Locked` both for the third wrong PIN and for subsequent locked-out inputs, and `Invalid` also covers syntax. Instrument the actual comparison outcome, not just return enums. Include a wrong comparison even if the subsequent failure-counter persistence write fails; report storage trouble without altering existing result/lockout behavior. Emit once from a central comparison-result hook with bounded non-secret metadata. `AdminPin::change` also uses `verify`: a wrong current PIN in an already-authorized Owner PIN-change request qualifies with source `OWNER_MANAGEMENT`; the unauthenticated route must still reject before verification. The existing PIN failure counter is security state and must remain durable.

Use the already-loaded identity in Access; do not add an SD lookup after unlock. Event snapshots never contain claimed-but-unverified identities. A physical PIN authenticates permission, not a named person.

Fold `ADMIN_LOCKOUT` into the third wrong-attempt message; no additional request. Do not notify every normal relock, boot or Wi-Fi change. No physical relock-failure detector exists: LockController's timer/GPIO state is not magnet/door feedback. Do not invent such an alert or change the lock controller for this phase. Owner-requested test messages use a separate administrative test command, the same quota and sender, and are clearly labeled tests.

## 9. Message formats

One text object, one recipient, one push per event. Examples below are illustrative, not observed events:

```text
🔓 SmartLock ปลดล็อกสำเร็จ
อุปกรณ์: SmartLock ห้องหลัก
เวลา: 27/09/2026 21:35:12 (UTC+07:00)
ผู้ใช้งาน: ko
รหัสผู้ใช้งาน: D000001
สิทธิ์: OWNER
วิธี: Access QR / LAN
ผล: สั่งปลดล็อกสำเร็จ
```

```text
⚠️ ตรวจพบการใส่รหัส Admin ผิด
อุปกรณ์: SmartLock ห้องหลัก
เวลา: 27/09/2026 21:38:04 (UTC+07:00)
ผิดครั้งที่: 3
สถานะ: สั่งล็อกอยู่
ระงับการลองรหัสชั่วคราว 60 วินาที
```

```text
🚨 SmartLock ปลดล็อกฉุกเฉิน
อุปกรณ์: SmartLock ห้องหลัก
เวลา: ยังไม่ยืนยันเวลา (uptime 123 วินาที)
วิธี: เมนู Admin บนอุปกรณ์
ผู้ดำเนินการ: ไม่ระบุตัวบุคคล
ผล: สั่งปลดล็อกสำเร็จ
```

Wrong-PIN lock status must reflect the captured logical state: use `สั่งปลดล็อกอยู่` if unlocked, never hardcode “door remains locked.” Add `แจ้งเตือนล่าช้า` with age when delivery starts over 30 seconds later; freeze the resulting text on first submission for identical retries. Messages describe commands/state, not measured physical movement.

Reuse TimeManager's NTP-confirmed epoch and quality at event creation; its current validity window is 24 hours since synchronization. Preserve unknown time if no trustworthy epoch existed then, even if NTP becomes valid before sending. Retain TimeManager when Audit is removed. Format UTC epoch at UTC+07:00 for Thailand and explicitly show offset. A notification-only offset setting can support other locations (default +420 minutes); do not change global authorization clocks. No DST claim for a fixed offset. Uptime is labeled uptime and may wrap; queue age uses wrap-safe elapsed subtraction.

JSON-escape quotes, slashes, newline and control characters; reject invalid UTF-8 and truncate display strings at character boundaries. Strip label/name control and bidi formatting characters that could impersonate other fields. Never render arbitrary strings as JSON structure. Max rendered text 1,024 UTF-8 bytes, serialized request 2,048 bytes; overflow fails locally and increments loss, never spills into another request.

## 10. Device naming

Notification-only friendly label, at most 64 UTF-8 bytes plus terminator, Owner editable. Empty label falls back to `SmartLock-04225A0FF0A4` derived from the current unit identifier. Keep `InstallationMetadata` and canonical hostname unchanged. Label is not authorization, hardware identity or a general renamed product field. Snapshot it per queued event so later edits do not alter pending content.

## 11. Token/authentication design

| Token | Embedded tradeoff | Selection |
|---|---|---|
| Long-lived | No automatic renewal machinery; indefinite compromise window until revoked | Select for dedicated one-unit OA |
| v2.1 | At most 30-day lifetime; JWT private key, key registration and renewal required | Reject for this minimal standalone phase |
| Short-lived | 30-day renewal; needs channel secret to renew, retaining another powerful credential | Reject |
| Stateless | 15-minute renewal and non-revocable issued tokens | Reject |

Persist only token, recipient ID, enabled flag, label, timezone offset and config version in a dedicated namespace such as `sl-line`. No channel secret, signing key, Google token or LINE password. Cap token input at a validated implementation limit (initial budget 512 bytes; check actual long-lived format before L1 acceptance). No token readback endpoint, localStorage copy, Serial echo, URL, source literal, test fixture, command argument, Git file or dump. Worker gets a bounded private configuration snapshot; wipe temporary buffers after use. Configuration commits are versioned/atomic and read back privately; an interrupted update keeps old valid config or disables notifications, never damages auth namespaces.

**NVS is not automatically encrypted.** Current build flags do not establish flash encryption, secure boot or encrypted NVS. Treat physical flash extraction as token compromise. Do not enable irreversible eFuses or repartition as part of this feature. A dedicated OA limits impact, but a stolen channel token remains capable of sending as that OA beyond the configured recipient. It grants no local door access. Owner revokes/reissues it in LINE settings and reprovisions over USB if compromised.

Select trusted-local-computer USB provisioning with a masked local helper. Future firmware accepts only a bounded notification-config operation during a short, explicitly activated, already PIN-authorized physical Admin maintenance window; it cannot read or mutate PIN/auth/Wi-Fi or drive GPIO. No always-open Serial writer. This narrowly scoped setup window is a necessary new notification provisioning control, not a change to PIN verification or Admin authorization policy. SOL must review this new boundary before implementation. The token stays in helper memory, travels over direct USB, and is never printed. If tooling cannot transfer secrets without transcript capture, the user enters the token directly into the masked helper; never into chat. Existing HTTP Management is unsuitable for transferring a valuable bearer token. No fake encryption with a key stored beside ciphertext.

Local disconnect disables worker, invalidates configuration generation, wipes token/recipient and pending RAM records; it does not automatically revoke the remote token. A request already accepted cannot be recalled. Token replacement and recipient change drop old-generation queue records, counting them as discarded, so private events never transfer to a new recipient accidentally. Sender checks generation before sending and before applying results.

## 12. Recipient/user-ID design

Use the intended Owner's **own developer user ID**, obtained from this channel's Basic settings after linking the developer Business ID to the same LINE account. Add this OA as a friend using its official friend QR/link. Provision the `U` plus 32-hex user ID over the same helper. Do not use phone number, display name, public LINE ID, group ID or an ID from another provider.

This is deliberately one Owner who can administer the channel. It is not a general consumer self-service pairing flow. If a different recipient is needed later, reassess supported identity discovery rather than guessing IDs. Webhook discovery and verified-account friend enumeration are unnecessary here. A test notification followed by Owner confirmation is required to establish receipt; HTTP acceptance cannot prove friendship or unblocked delivery.

## 13. LINE account setup

Future setup: log in with intended LINE-linked Business ID; create a dedicated Thailand Free OA; enable Messaging API in OA Manager; choose the Owner's provider carefully; inspect channel in Developers Console; issue one long-lived token; obtain own user ID; add OA as friend; provision with helper; send one labeled test. Disable unused webhook/auto-reply behaviors; no webhook URL is configured. Do not purchase verified/premium identity, premium ID, add-ons or another plan.

Codex may perform ordinary configuration after authorization; account login, MFA, terms/ownership confirmation and physical provisioning authorization remain user actions. Do not claim one-click LINE Login accomplishes channel setup. L0 has performed none of these steps.

## 14. TLS/network design

Fixed `api.line.me:443`, exact allowlisted push/quota paths, TLS 1.2 minimum, SNI and certificate hostname/validity verification. No redirects, insecure mode, custom destination field, HTTP fallback or leaf fingerprint pinning. Prefer a small reviewed PEM root set using built-in `WiFiClientSecure`; initial candidate is the currently validated DigiCert Global Root G3, fetched from the CA's official certificate repository during implementation and independently checked. Recheck all observed chains before deployment. A curated fallback root is added only for a verified alternate chain, not copied from old Google code. Root rotation may require a future ordinary firmware update; fail notification delivery safely until repaired.

Wait for plausible NTP-confirmed time before opening TLS. SNTP is not cryptographically authenticated; clock manipulation can disrupt service and is a limitation, not permission to disable certificate checks. Preserve existing time synchronization policy. Current `time.cloudflare.com` is a public NTP server, not the abandoned Cloudflare Worker architecture; no new Cloudflare service is introduced.

Worker: one request at a time, no parallel TLS connections, lower priority than local control, no network I/O on Arduino loop or timer callbacks. Target DNS/connect 3 seconds, TLS handshake 5 seconds, response read 3 seconds, **12-second total attempt ceiling**, with an absolute deadline unaffected by trickled bytes. If framework DNS cannot meet that ceiling, use an asynchronous/cancellable resolver or defer the send; do not accept an unbounded worker operation. Bounded response body 2 KB and headers 2 KB, stream-discard irrelevant fields, close on overflow. Do not log raw error bodies or request headers.

Pause new attempts during STA transition, Recovery-AP-only operation, low heap, provisioning or reset. Resume on healthy home STA plus time. Do not require “lock locked” to start sending—real-time unlock notifications should send while it is released without affecting the independent relock timer. Network/queue tasks do not acquire SD locks. A stuck/cloud-failed worker is isolated; restart its connection only after controlled cleanup, never reboot the SmartLock just for LINE.

## 15. RAM queue design

Fixed 16 slots, compile-time record budget at most 256 bytes/slot (4 KB total), plus one in-flight record/body. Store type, UUID retry key, timestamp/quality, monotonic enqueue age, bounded identity snapshot, label, source, PIN failure metadata and config generation. Recipient/token are configuration, not event fields; an in-flight snapshot binds the exact recipient/generation. One immutable formatted body per in-flight event is retained across retries.

FIFO normally. On full queue, discard the oldest waiting record, never overwrite the in-flight record; increment RAM loss and admit the newest event. No per-type coalescing or emergency double-send. Every enqueue is bounded and nonblocking. Queue-disabled status counts qualifying events as not sent this boot, without retaining payloads. A transient generation change cannot cause use-after-free or send under mismatched credentials.

Time-to-live **one hour from event creation**, including offline and quota-paused time. Maximum 20 actual HTTP submission attempts. TTL applies to all slots, not just the head. If the in-flight deadline expires while I/O is running, safely finish/close the connection before releasing its buffer; a late accepted result can retire that event but cannot mutate a reused slot. Counters are saturating RAM-only: accepted, expired/dropped, pending, last status and last acceptance time. They clear at boot; no per-event NVS writes.

## 16. Retry/failure behavior

Try promptly, then delays of 5, 15, 30, 60, 120, 300 seconds, capped at 300 with bounded jitter; stop at TTL/attempt cap. Apply outage/backoff globally so a queue of events does not multiply requests during a service failure. No busy loop; no retry when disconnected. Limit ordinary starts to at most one push/second despite much higher API limits.

Generate UUIDv4 retry key with ESP hardware randomness before first request; send `X-Line-Retry-Key` on every attempt. Same key, recipient and body for all retries, always within the documented 24-hour window. No stable SD event ID is required for this RAM-only design.

| Outcome | Action |
|---|---|
| Complete TLS-verified 200 with structurally valid bounded push response | Mark API accepted, remove queue record. Do not claim delivered/read. |
| 409 with expected duplicate-acceptance semantics and valid `x-line-accepted-request-id`, in response to this exact keyed push | Mark previously accepted; remove. Never accept arbitrary 409 as success. |
| Timeout/lost response/5xx | Keep exact body/key and retry within bounds; initial send may already have succeeded. |
| Malformed/oversized/truncated response, wrong host/TLS failure | No success; retain and back off. |
| 400 invalid payload/recipient | Drop that invalid event with loss count and safe error; persistent configuration errors pause sender. |
| 401/403 | Pause authentication, show reconnect/reprovision required; queue ages out normally. No endless attempts. |
| 429 | Recheck quota; monthly exhaustion pauses push, rate/concurrency restriction backs off. Unknown cause uses conservative pause/backoff, never assumes acceptance. |
| No Internet/DNS/time | Local operation unaffected; wait/backoff/expire in RAM. |
| OA blocked / user account deleted | API may accept without delivery. No truthful automatic delivery proof; Owner test/check required. |
| Reboot/power loss | Queue and counters vanish; no replay reconstruction or persistence. Accepted messages remain in LINE. |

Unlike the old Google receiver, LINE returns no seven-field event receipt or SD-retirement authorization. A valid response is linked to the worker's exact request/key and trusted endpoint. Never reuse Google receipt parsers. Rare ambiguous permanent failure may mean a message arrived even though this boot's status says unknown/lost.

## 17. Free quota handling

Query `GET /v2/bot/message/quota` and `/v2/bot/message/quota/consumption` on startup/reconfiguration and before the first push. Cache results; refresh every 15 minutes while active, after relevant 429, and after extended reconnect. Show usage as approximate, not a definitive bill. Local counter conservatively reserves one potential send on first submission (not again on retry); delayed server statistics cannot make the counter go backward during the same observed quota period.

Require limited quota metadata and an approved Free-account setup record. Local maximum is 300 even if an account is accidentally upgraded. An unexpected plan/limit increase or unlimited response pauses for configuration review. Metadata cannot conclusively prove billing-plan identity; actual Free plan/no-payment configuration is the cost safeguard, not arithmetic alone.

At exhaustion show `โควตาข้อความ LINE เดือนนี้เต็มแล้ว`, stop pushes, and refresh quota at most once per six hours. New events still pass through bounded queue/TTL and loss accounting; they are not saved for next month. Resume only after confirmed available quota from LINE; do not reset counters on reboot, guessed Bangkok midnight, or a local clock change alone. After reboot, fetch server usage before sending. Because consumption is approximate, conservative reservation can stop early; choose missed notifications over deliberate overage. Free-plan server enforcement prevents paid delivery. If quota cannot be established, defer sending and show status, with no paid fallback.

One event/one recipient/one request is one counted send when delivered under LINE accounting. The design uses one text object even though multiple objects do not increase recipient-based count. Test button consumes quota and is rate-limited (one per minute) to avoid accidental waste. No broadcasts or group recipients.

## 18. SD/EventLog removal plan

**Plan only; do not execute in L0.** Source audit establishes these boundaries:

| Current component | Later action / protected dependency |
|---|---|
| `src/events/EventLog.{h,cpp}` | Remove persistent CSV, `/smartlock/outbox` segments, sequence/CRC/export/retention/pinning/ACK/history-loss machinery and obsolete `google-v1` marker from active code. |
| `src/events/Audit.{h,cpp}` | Replace emission side effects with RAM security emitter; remove non-selected history-only calls. Do not remove their enclosing auth, enrollment, network or reset decisions. |
| `Audit::granted`, AccessController | Currently emits AccessGranted + Unlock and rereads identity. Replace with one RAM notification from verified record after actual unlock. Preserve all authorization/session consumption. |
| `main.cpp`, physical Admin branch | Preserve unlock gates and LockController ordering. Emit Emergency event only on success. Remove historical boot/drain/relock bookkeeping, not timer control. |
| `AdminPin::verify/change` | Add non-secret comparison outcome hook; retain verifier, persistence, failures and lockout exactly. This security counter is not operational history. |
| `Audit::update` / `TimeManager` | Keep SNTP/time-quality functionality via independent update call; removing Audit must not silently disable time synchronization. |
| `Audit::pollDiagnostics` | Extract/retain generic `DIAG_AUTH` and `DIAG_SYSTEM` bounded read-only diagnostics. Remove `DIAG_AUDIT` CSV export; optional RAM-only notification summary contains no secrets. |
| WebServerManager `logsStatus/logsSegments/logsExport` and `/api/logs/*` | Remove historical routes and handlers; return unavailable/404, no empty-success pretend history. |
| `src/web/WebAssets.h` | Remove `บันทึกการใช้งาน`, export and history UI/JS. Add only current-boot LINE status. |
| `src/storage/StorageHealth`, SD mount | KEEP mount, card health, fail-closed storage behavior, SPI ownership and all auth repair checks. Empty history-directory creation may be removed only after path audit. |
| `/smartlock/db/identities.rec`, `identity-mode.rec`, `auth.rec` | KEEP, including AtomicFileStore transaction/recovery artifacts. These are authoritative security data. |
| `/smartlock/backups/identity-v1`, legacy users/devices records | KEEP existing migration/integrity behavior. IdentityStore uses backup auth data for verifier-preservation checks; not event history. |
| ConfigStore, NetworkSecrets/StaSecrets, InstallationMetadata, touch calibration | KEEP NVS configuration and authentication-sensitive namespaces; no redesign. |
| FactoryResetController | Keep original physical/session authorization and lock-first sequence; add future notification-config erasure and worker cancellation. No reset to migrate logs. |

Existing files must not be erased merely by booting a new build. First migration disables all event writes and treats old history as inert. A later explicitly authorized one-time data purge may remove only positively identified event files under `/smartlock/outbox` and any separately verified old audit CSV location, plus history-only `sl-audit` state. Inventory exact names before deleting; unknown files stay untouched. Never delete `/smartlock`, `/smartlock/db`, whole `/smartlock/backups` or format SD. “Future history NONE” means no active storage; achieving zero residual historical bytes on the installed card requires that separately gated purge. This distinction prevents accidental data destruction.

Replace EventLog-only tests; keep auth, SD corruption/recovery, HTTP, network, Admin and lock tests. Remove inclusion of EventLog enums from remaining components by mapping needed LAN/Recovery/physical source metadata into a tiny notification-owned enum. Do not leave a shadow EventLog or persistent queue in NVS.

## 19. Management UI

Owner-only, preserve existing Management session expiry and ingress checks; ADMIN/USER/GUEST cannot inspect recipient/configuration or trigger notification controls. Use existing authenticated mutation POST convention; no state-changing GET.

```text
ระบบ → การแจ้งเตือน LINE

ยังไม่ได้ตั้งค่า
[ ตั้งค่าการแจ้งเตือน LINE ]
  ตั้งค่าครั้งแรกผ่านคอมพิวเตอร์และสาย USB
  ไม่ต้องตั้งค่าเพิ่มเติมในการใช้งานประจำวัน

พร้อมส่งการแจ้งเตือน   (does not mean delivery confirmed)
อุปกรณ์: SmartLock ห้องหลัก
ข้อความเดือนนี้ (ประมาณ): xx / 300
รอส่ง: N
ส่งให้ LINE ล่าสุด: ...
แจ้งเตือนที่ส่งไม่สำเร็จในรอบการทำงานนี้: N
[ ส่งข้อความทดสอบ ] [ ตั้งชื่ออุปกรณ์ ] [ ยกเลิกการเชื่อมต่อ ]
```

Additional truthful states: `รออินเทอร์เน็ต`, `รอยืนยันเวลา`, `ตรวจสอบโควตาไม่ได้`, `ต้องตั้งค่าการเชื่อมต่อใหม่`, `โควตาข้อความ LINE เดือนนี้เต็มแล้ว`. Explain counters reset at reboot and queued messages may be lost. No historical list or CSV export, token entry/readback, developer endpoint settings, or fake LINE OAuth button. Recipient can be displayed as a masked identifier; no public profile lookup dependency. Disconnect confirms notification-only effect; it does not revoke local access or remote OA ownership.

## 20. Security boundary

Outbound notifications only. No webhook, poll-for-command loop, LINE login authority, unlock route, remote PIN/reset/firmware controls or URL from a message. Sending a chat to OA cannot reach SmartLock. Network worker's response parser exposes only bounded notification outcomes; arbitrary fields cannot become local commands.

Exclude entered PIN, hashes/verifiers/salts, browser credential, session/QR/enrollment/Management tokens, Wi-Fi/AP passwords and all secrets from event fields, logs and messages. Recipient ID and identity names are personal data: show only to authorized Owner; remote LINE necessarily receives configured notification content. Phone lock-screen previews can expose it. OA channel token theft allows impersonation/spam/quota exhaustion, not door access.

Future authorized Factory Reset cancels sender and clears `sl-line` configuration and RAM, while preserving existing reset authorization and calibration policy. It must not contact LINE or depend on Internet, delete chat history/OA, or silently provision again. A stolen token remains revocable through LINE settings after local reset. Ordinary reboot reloads configuration but never resurrects events.

## 21. Firmware resource impact

Clean baseline from cleanup authority: actual application binary 1,089,200 bytes; slot 1,310,720; headroom 221,520; static RAM 106,496. This is not a new build measurement.

Planning estimates: LINE client/UI/config/queue plus linked TLS may add roughly 60–150 KB flash before EventLog removal savings; exact link growth is unknown. Budget 4 KB queue, 2 KB request, 2 KB response, 2 KB header cap, 6–8 KB task stack and approximately 40–70 KB transient TLS heap. Avoid allocating full header/body limits multiple times; account for library internal buffers. Config and formatting add a few KB. No SDK or large dynamic JSON tree.

L1 must measure actual binary, minimum free heap, largest allocatable block and stack high-water mark under concurrent HTTP/QR/touch work. Initial sender admission target: at least 90 KB free heap and 40 KB largest block before TLS; tune from measurements without weakening lock/network safety. If budget fails, shrink only new buffers or defer notification work, not partitions or authorization. Preserve meaningful heap reserve after handshake. No promise of resource PASS until target build and later hardware tests.

## 22. USER ACTIONS

One-time: authenticate to LINE/OA/Developers as needed, approve account ownership/terms, use intended linked LINE account, add dedicated OA as friend, authorize the limited USB setup window with existing physical Admin, and confirm one test arrives. If safe secret transfer automation is unavailable, paste token directly into masked local helper, never chat. No coding, JSON, firmware editing or paid setup.

Normal daily use: **no action**. Exceptional maintenance: unblocking OA, token revocation/replacement, changed LINE policy or CA update may require setup assistance. This is not a consumer one-click login integration.

## 23. CODEX ACTIONS

After separate authorization: automate ordinary OA/channel configuration where supported, verify Free/no-payment state, prepare secure local helper, implement fixed outbound client and RAM queue, Thai UI and narrowly reviewed event-history removal, run tests/build, and report measurements. Secret-handling limitations must be respected rather than printing console/token pages into transcripts. Account-owner authentication stays user-operated.

Deployment, real token issuance, test sends and any historical SD purge require the corresponding later phase. No COM6, accounts or production changes in L0.

## 24. SOL/LUNA allocation

SOL owns architecture acceptance, secret provisioning boundary, event semantics, source-by-source EventLog removal approval, PIN-hook review, lock ownership, TLS/response parsing, cost controls and final acceptance. SOL reviews every major diff and full integrated diff.

LUNA HIGH implements bounded approved tasks: queue/client/helper/UI, removal of enumerated event-only code, focused tests, builds and evidence. Luna does not approve security, independently delete SD data, issue tokens or deploy hardware. This L0 document assigns future work; no worker implementation was launched.

## 25. Implementation phases

1. **L1 — source + host tests only:** SOL reviews design boundaries; Luna implements RAM events/queue/client interface, secret-safe provisioning helper and notification-only config, UI, exact EventLog code removal. Mock LINE; no real account/token/send/COM6/data purge. Compile and review all regressions.
2. **L2 — account and staging acceptance:** separate authorization; dedicated Free OA, intended recipient, token provisioning rehearsal with disposable test device/config; real minimal test sends and TLS/quota checks. User confirms receipt. Never exhaust real quota to test limits.
3. **L3 — hardware acceptance:** separate authorization; baseline deployed state first, reviewed normal flash without reset; verify retained identity/PIN/Wi-Fi/calibration/SD database and lock state. Exercise notification failures concurrently with safe physical tests. Mark actual movement/receipt separately.
4. **L4 — optional old event-data purge:** explicit data-deletion approval and exact path inventory; remove only retired history if user wants zero residual bytes. No Factory Reset. Can be omitted while old files remain inert.

## 26. Test plan

| Layer | Required evidence |
|---|---|
| HOST — production event hooks | One actual unlock → one event; already unlocked, denied, replay, revoked → no success event; emergency → one special event; wrong PIN 1/2/3 and third-attempt lockout; locked-out inputs do not emit; PIN-change wrong-old-PIN source correct; storage-write failure after wrong comparison still emits safely. |
| HOST — security isolation | Failed/full/disabled queue cannot alter Access return, session consumption, timer or GPIO ownership. No new SD lookup in enqueue; bounded execution/no allocation in emitter; real Admin authorization unchanged. |
| HOST — queue/time | FIFO/overflow/in-flight protection, TTL, 20-attempt cap, millis wrap, unknown NTP, delayed send, UTF-8/control/JSON escape, label changes, config generation/disconnect, no retry-key mutation. |
| MOCK RECEIVER — transport | 200/409 accepted validation, dropped response after acceptance, identical retry, malformed/truncated/oversized responses, 400/401/403/429/5xx, quota exhaustion/reset approximation, DNS stall, slow TLS/body absolute deadline, forbidden redirects, invalid CA/host/expiry. No accepted status from arbitrary JSON. |
| HOST — storage removal | SD spies show no event writes anywhere; NVS spies show no event/queue writes; identity/auth atomic recovery and migration backups unchanged; history routes/UI gone; generic read-only diagnostics/time preserved; reset erases only intended new config in addition to old policy. |
| BROWSER | Actual production Thai Management assets; Owner-only route controls, revoked/expired denial, CSRF/session behavior unchanged, no token readback/input over HTTP, test-rate limit and quota/offline status, no historical page. |
| BUILD / resource | Firmware slot and static RAM; root/endpoint/source scans; dependency and secret scans; no Google/relay/LINE inbound commands; protected-source diffs reviewed. |
| LIVE ESP32 — later | TLS heap/minimum heap/largest block/task stack; concurrent Access/HTTP/QR/touch/relock; network loss and reconnect; no SD event growth; normal reboot retains config/auth but clears queue. No unsafe forced reset or quota exhaustion. |
| REAL LINE — later | Free plan verified, friend recipient and test received, valid key retry produces no duplicate, revoked test token pauses, no real quota attack. Use controlled fixtures for errors not safely induced on account. |
| PHYSICAL USER — later | Actual magnet unlock/relock remains correct; wrong-PIN and emergency messages visibly arrive with truthful attribution. Software GPIO evidence alone cannot certify door movement or human receipt. |

No L0 tests are claimed for code that does not exist. L0 verification consists of repository/source review, official research and unauthenticated TLS handshake only.

## 27. Risks/limitations

- Free quota and attacker-generated PIN failures can suppress later notifications. No reserved paid capacity or silent batching defeats that fact.
- RAM queue loss on restart, expiry and overflow is intentional; this is not an audit trail or an alarm delivery guarantee.
- API accepted does not mean phone delivered/read, especially when blocked; no per-message read receipt in this design.
- Long-lived token can be extracted from an unencrypted device; dedicated OA and revocation limit consequences but do not prevent physical extraction.
- Setup requires OA/channel developer configuration once. Codex can assist; no unsupported “sign in and done” claim.
- Single curated trust-root set requires maintenance on chain changes; network/time faults can pause delivery.
- Existing Management HTTP remains its current trust limitation; do not widen it by transporting LINE bearer credentials.
- Removal of EventLog eliminates local forensic history and future backfill. Existing SD auth dependence remains; “no event history” does not mean “SD can be removed.”
- Named Access identities are verified; physical Admin operator identity is unknown. Logical lock output is not a door-position sensor.
- Plan terms may change after this research date. Recheck official Free limits immediately before account setup and deployment.

## 28. Exact next instruction for SOL

> Begin LINE L1 — implementation and host verification only, using this architecture. Review USB provisioning and PIN-comparison hooks before assigning bounded work to Luna High. Implement the direct outbound client, 16-slot RAM queue, three security events, Owner-only Thai notification controls, and approved event-history code removal while preserving all SD authorization data and local security behavior. Use mock tokens/receiver only. Do not create LINE accounts, issue real tokens, send real messages, access/flash COM6, purge existing SD files, change partitions/eFuses, reset, or alter Owner/PIN/Wi-Fi. Report source audit, regression/build/resource results for SOL review; stop before L2 account setup or hardware authorization.

L0 conclusion: architecture feasible for zero-cost, best-effort security notifications within the current Free plan. **Implementation and hardware acceptance remain unperformed.**
