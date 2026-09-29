# LINE-only Owner provisioning architecture

## Active amendment: physical-only LINE maintenance

The subsequent explicit user instruction supersedes the Owner-web-arm prerequisite below. A PIN-authenticated user selects a dedicated LINE setup screen on the physical device while logically LOCKED. That screen grants only a fixed 120-second, one-use, installation-bound USB LINE configuration transaction. It grants no browser Owner authority. Its cancel/expiry/success closes the authority; Emergency Unlock and reset actions are unavailable on that screen. Existing PIN verification/lockout and other Admin timeouts remain unchanged. HTTP arming routes are removed. Management reconfigure shows instructions only. After a successful commit the existing RAM notification engine queues one test message; no network operation is in the authorization or lock path.


Status: DESIGN ONLY. Date checked: 2026-09-28, Asia/Bangkok. Architect: ASTRA.
Authority for the next implementation; supersedes the earlier USB/HTTP provisioning decisions, not the existing notification event semantics. No firmware, account, secrets, COM6 or Git commits changed by this design.

## 1. Executive summary

Keep direct ESP32 HTTPS Push to one Owner through a dedicated LINE Official Account (OA). Choose **Option A: Codex-assisted USB provisioning**, gated by current Owner Management authorization and an existing physical Admin confirmation. Remove raw credential fields and the network credential-save handler completely. Normal Management offers a public Add Friend link, test, label and status only. No relay or cloud database is required.

This is feasible for this one installation because the intended recipient participates in initial LINE developer-account setup. It is not automatic enrollment of arbitrary OA friends. Codex performs technical setup; the human performs login/MFA/terms, physical confirmation, adds the OA and confirms receipt. Secret transfer automation has an explicit capability gate; never claim a browser tool can safely transfer secrets until that path is tested with synthetic values.

Inspected baseline: HEAD f1bdc713d3a880a02defb3492d77239156a74b01; line-notifications-v1 points to 064518abc55002a121a03c7b848a2e635a9afb78. Deployment evidence: docs/phase_reports/LINE_NOTIFICATIONS_IMPLEMENTATION_REPORT.md. Recorded binary 1,224,528 bytes, headroom 86,192, static RAM 112,348. These are previous deployment measurements, not a new live-board check.

## 2. Why no Cloudflare/D1/server is needed for this device

The recipient is known once during setup, so the device needs neither incoming messages nor identity discovery. LINE accepts authenticated outbound Push requests. Configuration lives on the device; retries live in RAM. LINE itself transports and retains chat messages under its own policy. No separate history service, webhook, OAuth callback host or relay exists.

## 3. LINE platform constraints

Official research checked 2026-09-28; no account was opened or modified. Source facts are separated from the design decisions below.

| Official fact | Source |
|---|---|
| LINE Notify ended 2025-03-31. | https://notify-bot.line.me/ |
| Create an OA, then enable Messaging API in OA Manager; direct Messaging API channel creation in Developers Console is discontinued. | https://developers.line.biz/en/docs/messaging-api/getting-started/ |
| Developer's own ID is in channel Basic settings → Your user ID, only with LINE-linked Business ID; no API retrieves that developer field. IDs differ by provider, not by channel within a provider. | https://developers.line.biz/en/docs/messaging-api/getting-user-ids/ |
| Follow/message identity arrives through webhooks. Follower-list API is restricted to verified/premium accounts; it is not chat-code retrieval. | https://developers.line.biz/en/docs/messaging-api/getting-user-ids/ |
| HTTPS OA profile links support Basic IDs; percent-encode the ID. Desktop can show an OA QR. Deprecated line:// is unnecessary. | https://developers.line.biz/en/docs/messaging-api/using-line-url-scheme/ |
| Long-lived indefinite; v2.1 up to 30 days; short-lived 30 days; stateless 15 minutes. | https://developers.line.biz/en/docs/basics/channel-access-token/ |
| Push endpoint: POST https://api.line.me/v2/bot/message/push. Friends are eligible; blocked/deleted users may not receive messages even after 200. Push rate limit: 2,000 requests/second/channel. Text limit 5,000 characters; up to five message objects/request. Quota and consumption endpoints are supported. Long/short token revocation: POST /v2/oauth/revoke. | https://developers.line.biz/en/reference/messaging-api/nojs/ |
| Thailand Free lists 300 messages/month and no additional-message purchase under Free. | https://lineforbusiness.com/th/service/line-oa-features |
| Push counts by recipients; monthly excess prevents sending. | https://developers.line.biz/en/docs/messaging-api/pricing/ |
| Stable UUID retry keys identify repeated accepted requests for 24 hours; acceptance is not proof of delivery. | https://developers.line.biz/en/docs/messaging-api/retrying-api-request/ |
| LINE API deprecated TLS 1.0/1.1; use TLS 1.2 or newer. Verify applicable API-host announcement before deployment, not webhook-certificate requirements. | https://developers.line.biz/en/news/tags/ssl/1/ |

No supported webhook-free inbox polling API was found in the current Messaging API reference. This is a documented-interface conclusion, not a claim that no other LINE product can authenticate users. LINE Login/LIFF are separate application flows, not a replacement that lets this LAN device read a friend's pairing message.

## 4. Selected direct architecture

| Provisioning option | Decision |
|---|---|
| A. USB local helper | Selected. Small bounded firmware parser plus existing config store. Removes LAN secret transport and permanent credential web UI. Requires one physical setup window. |
| B. Owner HTTP form, hide/remove later | Fastest temporary reuse, but plaintext LAN secrets and two UI states/deployment steps. Merely hiding leaves a callable credential endpoint. Reject as final design. |
| C. Inject a generated NVS image / custom provisioning firmware | No network form, but risks collateral NVS changes, embeds secrets in build artifacts and complicates recovery. Reject. LINE Login/LIFF would add hosting/application lifecycle rather than simplify this one unit. |

```text
SETUP ONLY
LINE console --private local transfer--> trusted helper --USB--> sl-line NVS
Owner Management arm + existing physical Admin authorization ----^ gate

DAILY
local authorization -> LockController -> RAM event queue -> HTTPS api.line.me
                                                            -> Owner LINE
Management -> public OA Add Friend URL (no credential or pairing proof)
```

## 5. One-Owner assumption

Use the intended Owner's LINE-linked Business ID while viewing the actual Messaging API channel. Verify channel/provider/OA relationship, not just a similar display name. The person viewing Your user ID must be the intended recipient, not Codex's or a different developer's account. They need appropriate access to that channel; this does not require buying a provider or account verification. Recipient is provider-scoped and distinct from SmartLock D000001, public LINE ID, phone number, group ID and OA Basic ID. Never infer one from another.

## 6. First-time Codex-assisted setup

1. First implement/test firmware and the secret bridge using synthetic values. Do not issue real secrets before this passes.
2. Open official LINE login in the in-app browser. Human handles authentication, MFA, linking their LINE account, terms and ownership confirmations. No passwords/codes enter chat.
3. Codex creates/selects only the intended dedicated Thailand Free OA, enables Messaging API, verifies its provider/channel, and keeps webhooks disabled. Use Basic ID; decline premium ID/add-ons/payment. No invented business or legal details: human supplies genuinely required facts in the account UI.
4. Read the intended account's Your user ID privately from channel Basic settings. Obtain its public OA Basic ID from OA Manager; cross-check OA identity against the channel. Do not use an arbitrary chat participant or follower list.
5. Issue a long-lived token in Messaging API settings. Transfer it and the recipient directly to the trusted local helper without tool output, clipboard history, screenshots, traces, command arguments or transcript values.
6. Owner opens existing Management and arms LINE maintenance; human enters the existing physical Admin PIN on the device. Codex never enters/receives the PIN. USB helper commits only LINE configuration, closes the window, clears temporary buffers and reports booleans only.
7. User taps Add Friend and adds/unblocks this OA. Codex checks safe configuration status and requests one device test notification. Human confirms receipt in LINE. Record API acceptance separately from human receipt.

No daily setup follows. This design does not authorize executing these steps during L0.

## 7. User actions

Login/MFA/terms/account ownership; LINE-account linking if required; open Owner Management and confirm physical Admin maintenance; add OA as friend; confirm the test arrived. No code, console configuration choices, token copying, recipient-ID copying or JSON. Routine preferences use sensible defaults; ask only when required ownership facts cannot be inferred safely.

## 8. Codex actions

Automate safe navigation, OA/channel setup, provider checks, public ID collection, private credential transfer, USB provisioning, status/test checks, builds, review and deployment when separately authorized. Redact before outputs are created, not after transcripts exist. Use the user's existing authenticated browser session only through supported tools.

Feasibility gate: prove the available browser automation can move a secret from the permitted page into a local helper without serializing it into model-visible results. Prefer a scoped, reviewed local browser-to-helper bridge with no logging and no general remote access. Do not assume the current in-app browser API supports this. If unavailable, stop at that specific automation boundary and propose an approved local masked-input handoff; never silently switch browsers, request secrets in chat or claim fully automatic transfer. Account UI login/terms cannot be bypassed. Developer console screens can change.

## 9. Token strategy

Choose one long-lived token for a dedicated, single-purpose OA. Indefinite validity avoids a renewal service and additional client secret/private key on ESP32. This deliberately trades a longer compromise window for standalone maintenance simplicity. v2.1 adds signed assertion/key lifecycle; short-lived adds monthly renewal and channel-secret handling; stateless requires frequent renewal and cannot be revoked individually. None removes the need to protect a powerful issuing credential.

Store no Channel Secret or signing key on ESP32. For compromise, Codex revokes the old long-lived token using the supported revocation API/console while privately holding it, then issues and USB-provisions a replacement. Explicitly verify old-token rejection; do not rely on an assumed reissue grace period. Never include token material in evidence. Do not revoke unrelated channel tokens.

## 10. Owner user-ID strategy

Get Your user ID from the selected channel's Basic settings under the intended Owner's linked account. Enforce U followed by 32 hexadecimal digits. IDs from another provider are not interchangeable. Add this OA as friend before final test. User identity/account changes require a fresh developer-assisted recipient selection and provisioning; a public Add Friend click does not change it. A blocked OA can still receive API acceptance without user delivery: show no inferred friend/delivery guarantee. A profile check can assist diagnosis, but must not be represented as durable permission or message receipt.

## 11. Add Friend UX

Persist only the verified public OA Basic ID (bounded 64 ASCII bytes), not an arbitrary URL. Generate `https://line.me/R/ti/p/` plus percent-encoded Basic ID, including `%40` for @. Use the built-in public link; no paid premium ID, secret QR, callback, nonce or recipient ID. Show a QR of that same URL only if useful on desktop, reusing existing QR code support without an external QR service. Add Friend cannot leak the channel token.

Before the OA public ID is known, show no fake Add Friend button/QR: `ต้องตั้งค่าระบบ LINE ครั้งแรกผ่านผู้ดูแลระบบ`. Once known, allow the button even if notifications are disabled. Use escaped text and a fixed URL origin/path; never inject untrusted HTML or arbitrary schemes.

## 12. Secret provisioning method

Add one narrowly scoped USB configuration operation, not an arbitrary shell or NVS editor. Preserve existing read-only diagnostics.

- Owner-authorized POST arms a RAM-only maintenance capability for at most 120 seconds; it contains no LINE credentials. Do not grant it to ADMIN/USER/GUEST. Recheck current Owner authorization at commit.
- Commit also requires the existing physical Admin authorization to be valid and the logical lock already locked. Entering maintenance does not unlock or change lock timing. Honor the existing Admin inactivity timeout; do not extend it or change PIN semantics.
- Bind the transaction to a fresh random one-use nonce and unit identity delivered over USB, not a URL. Gate expires on timeout, reboot, cancel, role/session invalidation or successful write. The nonce prevents stale-frame replay; it is not encryption against a hostile USB host.
- Versioned, length-prefixed frame, maximum 1,024 bytes: nonce, token <=512 bytes, recipient 33 characters, label <=64 UTF-8 bytes, Basic ID <=64, timezone. Strict types/lengths/UTF-8; reject unknown versions, extra fields, incomplete frames. Limit processing per main-loop iteration and use a total two-second frame deadline. Never echo payloads.
- Validate the complete candidate before writing. Keep existing configuration on parse/storage failure; transactionally replace using NVS atomic blob semantics and verify privately. Clear candidate/frame buffers on every exit. Return only result/configured/version; no token, recipient, digest of secrets or config dump.
- Reuse v1 generation invalidation so queued/in-flight old-recipient retries cannot be reassigned. An already submitted request cannot be recalled. No generic USB reset/PIN/Wi-Fi/identity/unlock operation is added.
- Helper uses an exclusive COM port without automatic DTR/RTS reset, memory-only secrets, no shell arguments/environment variables/temp JSON files/serial transcript. Disable request bodies, browser tracing and crash dumps where practical; wipe buffers and exit after completion. OS swap, browser memory and host compromise remain limitations.

Implement a real atomic/readback-failure test; if current save() can write a candidate then fail readback, do not promise the previous config survives without a bounded storage fix or a documented disabled/fault state. This concerns LINE config only.

## 13. ESP32 NVS storage

Keep separate `sl-line`. Version 2 stores enabled, token, one recipient, label, public Basic ID and timezone; no event payloads, queue, last-message bodies or persistent loss counters. Keep field bounds explicit; adding Basic ID costs roughly 65 bytes plus layout padding. Migrate v1 by reading the old exact layout, preserving token/recipient/label and leaving Basic ID absent until confirmed. Never reinterpret a changed sizeof(Config) as an empty default and overwrite a working record. Unsupported/corrupt versions disable notifications without touching lock/auth state.

No proven flash/NVS encryption exists. Physical flash extraction can reveal the token and recipient. Separate namespace is organization, not encryption. No eFuse, secure-boot or irreversible hardware security changes in this phase. Logical disconnect deletes active config; wear-leveled stale flash may retain remnants, so remote revocation is necessary after compromise or disposal.

## 14. Normal Management UX

Owner-only `ระบบ → การแจ้งเตือน LINE`.

Unconfigured: `ยังไม่ได้ตั้งค่า`, setup explanation, public Add Friend button only if a valid Basic ID exists. No token/recipient fields, hidden HTML inputs or developer settings.

Configured: `พร้อมใช้งาน` means configured; separately display actual transport state, label, quota approximation and queue count. Buttons: `เพิ่มเพื่อน LINE`, `ส่งข้อความทดสอบ`, `เปลี่ยนชื่ออุปกรณ์`. Test success means `LINE รับคำขอแล้ว กรุณาตรวจสอบ LINE ของคุณ`, never delivered/read. Awaiting API response is explicitly queued.

Optional `ตั้งค่า LINE ใหม่` exposes only maintenance instructions and arming, never credentials. A confirmed Owner-only disconnect may disable/wipe local LINE config; explain that it does not revoke the remote token. Public Basic ID may remain for Add Friend, but Factory Reset clears it. Label change is its own Owner POST and must not require token re-entry or switch recipient. Keep current session/CSRF/no-store protections. Status returns no token/recipient; only public Basic ID/label and operational counters.

## 15. Re-provisioning/recovery UX

| Situation | Behavior |
|---|---|
| Token revoked/invalid | Pause; show `ต้องตั้งค่า LINE ใหม่ผ่านผู้ดูแลระบบ`; USB replacement after account checks. |
| Token rotated | Explicitly invalidate old token; replace complete candidate once; test. |
| Owner account/recipient changes | Return to intended account's channel/provider developer setup, collect its own ID, add OA, physically gated USB replacement. No automatic takeover. |
| OA blocked/unblocked | Human unblocks/adds it; test again. No automatic detection claim, recipient change or replay of already accepted messages. |
| Normal firmware update | Preserve namespace; migrate only known schema; temporary maintenance/queue do not survive. |
| NVS lost/corrupt | Notifications disabled; re-provision LINE only. Never Factory Reset to repair it. |
| Factory Reset | Clear LINE config and RAM state as part of existing authorized reset; no remote account/token deletion promise. Later revoke old token through LINE and re-provision if desired. |
| Lost access to original LINE administration | Account recovery with LINE is required; do not fabricate ownership or use another user's ID. |

## 16. Direct Push flow

Preserve existing production hooks: successful new Access unlock only, each completed wrong PIN comparison, successful physical Emergency Unlock. No denial/replay/AlreadyUnlocked/lockout-input notifications; third wrong comparison includes lockout once. Local action finishes before enqueue. Worker sends one concise Thai text to the sole recipient using fixed HTTPS API hostname, trusted CA, SNI/hostname validation and bounded I/O. No insecure TLS, arbitrary destination or leaf fingerprint. Recheck current LINE trust chain before live acceptance; CA certificate equality alone does not prove successful ESP32 TLS.

Only LockController writes GPIO22: HIGH locked, LOW unlocked. No webhook, polling chat commands, LINE login authority, cloud lock control or notification-dependent authorization.

## 17. RAM queue/retry

Keep v1's 16 slots including in-flight, FIFO, never overwrite in-flight; drop oldest waiting on overflow and increment RAM loss. Stable UUID and immutable body per attempt series; one-hour TTL, bounded backoff/max attempts. Require valid complete API response; accepted-request 409 is recognized only with LINE's accepted-request header. Expired/reboot-lost events are not replayed from SD. Authentication/quota errors pause; no retry storm. No behavioral redesign of the current event queue in this UX phase, apart from tests for configuration generation changes.

## 18. Zero-cost/quota behavior

Dedicated Thailand Free OA: 300 counted messages/month under current official table. No paid ID, add-ons, billing, overage or hosting. Query official quota/consumption; keep conservative local reservation and stop if quota is unknown or exhausted. Show `โควตาข้อความ LINE เดือนนี้เต็มแล้ว`. Periodically recheck supported quota endpoints for restored availability; do not assume midnight in Bangkok resets every LINE counter. Rate-limit tests locally. Daily use at ten required events/day can exhaust the month; wrong-PIN alerts and test sends also consume the budget. Local door operation continues. Real-time delivery of every event is impossible under finite quota/RAM/outages; generating each event is distinct from guaranteeing receipt.

## 19. Security limitations

Long-lived token theft permits OA impersonation, quota consumption and other channel-authorized API actions; it must not grant SmartLock access. Dedicated OA limits impact, not token powers. USB/host compromise and physical flash extraction remain risks. Local HTTP Owner sessions remain the existing accepted LAN risk, but LINE secrets no longer traverse that LAN form. API acceptance is not proof of friend status, notification display or reading. Do not persist screenshots with console secrets; do not emit recipient identifiers into reports. Credentials are not recoverable from a status API.

## 20. Why chat-code pairing is impossible without a webhook

A message-to-OA deep link merely opens chat. It does not return sender identity to this device. The documented Messaging API delivers incoming chat events through webhooks and offers no inbox poll that substitutes for this. Restricted follower enumeration neither retrieves a typed code nor authenticates which friend should own the lock. LINE Login could identify a consenting account under a separate app flow, but is unnecessary here and does not make chat-code pairing work. This design uses the already identified developer Owner and explicitly has no chat parser.

## 21. Implementation delta from line-notifications-v1

- `src/web/WebAssets.h`: delete token/recipient forms and save logic; add public Add Friend, label-only edit and bounded maintenance instructions/arm.
- `src/network/WebServerManager.{h,cpp}`: remove `/api/line/save` entirely, not just hide it. Keep Owner-only status/test/disconnect; add label and maintenance-arm POSTs without secrets. Reject old save route.
- `src/notifications/LineNotifications.{h,cpp}`: versioned config migration, public Basic ID/status, internal provisioning transaction and label update. Preserve direct sender, CA/time checks, queue and hooks.
- Add a small USB maintenance module integrated with existing serial dispatch to avoid two parsers consuming the same stream. Reuse existing PhysicalAdmin authorization predicate and Owner checks; preserve gestures, timeouts and PIN behavior. No maintenance command may invoke unlock/reset.
- Add a local helper/secret bridge and focused tests. No source token, generated NVS image, token fixtures with real data, raw endpoint backdoor or plaintext secret file.
- Keep `TimeManager`, Diagnostics, identity/auth SD operations, LockController and installed inert history. Do not restore EventLog/Google or change partitions. Current 86,192-byte headroom must be measured after build, not assumed.

## 22. SOL/Luna execution plan

ASTRA ends with this document. SOL owns gate/security, intended account verification, USB transaction review, migration failure semantics, source-wide secret audit and final deployment approval. LUNA HIGH implements bounded UI/config/helper/tests and browser navigation under SOL review.

Single pass: (1) baseline + synthetic bridge proof; (2) bounded code/UI/migration changes; (3) focused tests and build; (4) SOL integrated review/commit/checkpoint; (5) authorized state-preserving application deployment; (6) account setup with human authentication; (7) physical-gated USB provisioning, friend/test confirmation. Batch routine choices, avoid repeated approvals. Stop only for real account/physical actions or a proven safety/secret-transfer blocker. No claim of completion before actual human receipt. A future implementation instruction must authorize account creation, provisioning, one test send and deployment; this document alone does not.

## 23. Test plan

Use synthetic tokens/IDs first. Prove absent token fields/save route; Owner-only label/test/arm; ADMIN/USER/GUEST rejection; status/HTML/logs never expose secrets; malicious Basic ID cannot change URL origin or inject HTML. Test gate expiry, reboot, Owner revocation/session expiry, no physical authorization, stale nonce, replay, partial/oversize USB frames, parser deadlines and no lock coupling. Test v1 migration, v2 persistence, corrupt/unsupported config and failed write/readback without damaging existing auth state. Test disconnect/reconfiguration cancels old-generation retries.

Run existing focused event, queue/parser, PIN, Access/auth/identity, lock ownership and network regressions; build/size/secret scan. Browser mobile Add Friend navigation uses synthetic public ID until real setup. Live deployment compares non-secret boot/auth/PIN/Wi-Fi/calibration state without reading secret values into output. Real setup verifies intended channel/provider, Free plan, public OA link, API acceptance and separate human receipt; mock revoked-token/quota/blocked cases where live changes would be destructive. No forced unlock is needed to verify the test sender. No paid quota exhaustion experiment.

## 24. Exact next single-pass Codex command

> SOL lead, LUNA HIGH worker: implement docs/design/LINE_ONLY_OWNER_PROVISIONING_ARCHITECTURE.md against line-notifications-v1 in one bounded pass. First prove a private synthetic browser-to-helper transfer path; do not issue real secrets until it passes. Remove all normal token/recipient UI and the network credential-save route, add public OA Add Friend and Owner label/status/test controls, and implement physically gated Owner-authorized USB-only provisioning with safe v1 config migration. Preserve all door/auth/PIN/Wi-Fi/touch/SD authorization behavior and RAM-only notification semantics. Run focused tests/build/secret scan, SOL review and commit/tag. After SOL approves, deploy only the application to COM6 without erase/reset/partition changes and verify state preservation. Then use the in-app browser to configure one intended Owner's dedicated Thailand Free OA, enable Messaging API, privately obtain that account's provider-scoped Your user ID and a long-lived token, provision over USB, and send one test notification after the Owner adds the OA. Stop for login/MFA/terms, required Owner/physical confirmation and final received-message confirmation only; never request secrets in chat or bypass an unavailable private-transfer capability. No relay, webhook, database, paid plan, new persistent history or remote lock control. Report actual API acceptance and human receipt separately.
