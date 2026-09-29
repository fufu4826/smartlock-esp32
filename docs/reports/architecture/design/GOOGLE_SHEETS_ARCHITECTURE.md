HISTORICAL: Google Sheets is removed from the active product. This document records an earlier design or test checkpoint and is not current implementation authority.

# SmartLock Google Sheets architecture

> **HISTORICAL / SUPERSEDED (2026-09-27):** This design is retained as historical context. The approved current architecture is [Google Sheets one-click](GOOGLE_SHEETS_ONE_CLICK_ARCHITECTURE.md). This document does not describe the current approved design or establish deployment acceptance.

Design review, 2026-09-27. **DESIGN ONLY; not implementation approval or a cloud deployment.**

Source baseline: `9613716a0843b575a9cf3f90be8cbff3ae1c8032`. Reported deployed binary: 1,257,216 bytes; unchanged application slot: 1,310,720 bytes; headroom: 53,504 bytes; static RAM: 108,556 bytes. These figures are from the accepted defect-fix report, not a new build or board measurement.

Reviewed sources: `src/events/{EventLog,Audit,TimeManager}.{h,cpp}`, `src/cloud/{CloudSync,CloudProtocol}.{h,cpp}`, `src/cloud/GoogleRoots.h`, `src/network/WebServerManager.cpp`, `src/main.cpp`, `src/app/FactoryResetController.cpp`, `cloud/apps_script/Code.gs`, `platformio.ini`, and `docs/phase_reports/DEFECT_FIX_03_04_05_06_07_08_09_REPORT.md`. Existing unrelated report/evidence changes are outside this design.

## 1. Executive summary

Recommend **ESP32 -> small HTTPS SmartLock backend -> Google Sheets API**, using Google OAuth only in the backend. One service, one durable relational database, one background writer are sufficient. Do not introduce microservices, a message broker, Firebase SDKs on the ESP32, or a remote door-control service.

This is the simplest option that meets the desired consumer experience: Owner connects Google, consents, and receives an automatically created spreadsheet without copying URLs, tokens, spreadsheet IDs, or script source. Apps Script is simpler for a manually provisioned personal prototype, but does not meet that experience as cleanly.

Reuse the current EventLog as the only event source. Send immutable, CRC-validated closed segments. The backend reserves deterministic Sheet rows and acknowledges exact event IDs only after their content is verified in Sheets. Retry is safe after either side reboots or loses a response.

Recommend immediate retirement of a closed segment after a complete, verified Sheets acknowledgement (retention Option 1). While integration is enabled, unacknowledged segments must not be rotated away. This deliberately changes current bounded-rotation behavior: at full capacity, retain existing unsent records, report that subsequent events could not be recorded, and keep local door operation working. Finite storage cannot guarantee unlimited offline history. This tradeoff is explicit, not a hidden claim of lossless operation.

Two requirements need precise interpretation. Current firmware logs asynchronously after lock actions; it does not guarantee SD durability before physical actuation. This design guarantees **SD durability before cloud submission**, and preserves local operation if SD fails. Strict durable-write-before-unlock would be a separate local behavior change with a storage-failure policy; it must not be smuggled into CloudSync. Also, a separate ESP32 task isolates blocking network work, not arbitrary memory corruption or an MCU-wide panic.

## 2. Current EventLog architecture

| Property | Actual current behavior | Design implication |
|---|---|---|
| RAM queue | 16 events; normal enqueue copies bounded fields without SD I/O | A sudden power loss can lose queued events |
| SD history | `/smartlock/outbox`; maximum 64 segments, 32 records/segment, 8,192 bytes/segment | At most 2,048 records, often fewer when segments are partially filled |
| Segment identity | `E<8-hex-counter>_<32-hex-boot-nonce>.csv` | Use the entire basename, not just counter, as segment ID |
| Event identity | 128-bit boot nonce plus 32-bit event sequence, 40 hexadecimal characters | Preserve it exactly across retries; scope with installation ID |
| Validation | CSV header/schema and row CRC32; legacy and current rows supported | CRC detects corruption; it is not sender authentication |
| Traversal | Enumerate segments, retrieve a named segment, select oldest closed segment | Reuse these APIs; do not introduce another event database on ESP32 |
| Retention | Evicts oldest valid closed segment, excluding current and one pending-ACK pin | Today even unsent events can be evicted |
| Loss evidence | Saturating persistent event/segment loss counters in `sl-audit/loss` | Conservative: deletion failure/power interruption can overcount attempted loss |
| Deletion | Exact selected segment is validated before `deleteAckedSegment` | Keep exact-target checks; add protocol binding before calling |
| Timing | UTC timestamp if NTP is considered synchronized; otherwise empty timestamp and uptime | Never substitute upload time for occurrence time |

Current CSV header:

```text
event_id,timestamp,time_quality,user_id,device_id,action,result,network_mode,uptime_ms,claimed_device_id,access_source,crc32
```

The legacy header omits the last two attribution fields before CRC. `device_id` in these records means **browser identity D######**, not the physical SmartLock. `user_id` is a legacy relationship field, generally empty for the final identity model. Keep legacy input support but do not expose that confusing terminology to owners.

Current rows do not contain historical name/role snapshots, installation ID, firmware version, or separate actor/subject attribution. Do not fill those blanks from the current identity database and pretend they were captured at the event time.

`Audit::update` drains at most one queued event per interval; Access currently unlocks before `Audit::granted` queues ACCESS_GRANTED and UNLOCK. Timed relock creates RELOCK; physical emergency access creates EMERGENCY_UNLOCK_ADMIN_PIN and EMERGENCY_RELOCK. An event proves the software action, not measured magnet movement or door position.

## 3. Compared Google integration options

| Criterion | A. Apps Script web app | B. Small backend + Sheets API | C. Direct ESP32 + Google APIs |
|---|---|---|---|
| Implementation | Lowest for one manually configured owner | Moderate; pairing, OAuth, durable delivery ledger | Firmware complexity highest |
| Authentication | Shared ingestion key; deployment executes as owner or accessing user | Ingestion-only device key; Google tokens remain server-side | Autonomous uploads normally require refresh credentials/private keys on device |
| ESP flash/RAM | TLS + compact sender | TLS + compact sender; OAuth stays off ESP | TLS plus Google-specific authentication/refresh/error handling |
| Token handling | Script deployment permissions and shared secret | Encrypted refresh tokens; server handles expiry/revocation | Device must securely retain and rotate sensitive Google authority |
| Reliability | Script quotas, execution limits, deployment ownership, scan-based dedup issues | Controlled retries, durable row assignments and recovery | Device must handle every Google failure and token lifecycle |
| User setup | Manual deployment/URL/secret common | Click Connect, consent, automatic creation | Still requires hosted OAuth setup or awkward provisioning |
| Limits | Apps Script runtime/service quotas plus spreadsheet growth | Sheets API quotas and spreadsheet growth | Same Google limits, harder recovery on constrained device |
| Maintenance | Easy prototype; per-user deployments are awkward to update | One maintained service/domain/database | Certificate/API/OAuth changes may require firmware updates |
| Automatic Sheet creation | Technically possible under script execution identity | Yes, in consenting owner's account | Technically possible with suitable authorization |
| Friendly account connection | Possible only with additional deployment/add-on/auth design | Natural fit for hosted OAuth callback | Device alone does not solve friendly web account consent |

Apps Script execution identity is a real security distinction, not a substitute for per-owner OAuth. Its execution and service quotas also apply. See [web app execution permissions](https://developers.google.com/apps-script/guides/web) and [Apps Script quotas](https://developers.google.com/apps-script/guides/services/quotas).

Direct access is not inherently impossible. It is rejected because permanent Google authority and provider-specific lifecycle code would live on the least suitable component. A service-account key on the lock is not an acceptable shortcut.

## 4. Recommended architecture

Use one maintained backend deployment with HTTPS, a durable SQL database, and a serialized writer per spreadsheet. A small TypeScript service with managed PostgreSQL is a practical implementation choice; no special ESP framework is required. Hosting must support durable jobs or a scheduled worker; do not rely on an HTTP handler continuing after a serverless response.

Backend responsibilities: pairing, Google consent, spreadsheet provisioning, encrypted token storage, event validation, idempotency ledger, bounded pending jobs, exact Sheet writes, ACKs, and non-secret status. It has **no unlock, identity management, PIN, Wi-Fi, or local authorization API**.

Provisioning is itself a recoverable transaction. Persist a provisioning operation ID before creating a spreadsheet, attach a non-secret app-owned marker, and recover an ambiguous create response by finding that marker before creating again. Do not use the human-readable title as a unique key. Freeze delivery until exactly one destination is confirmed; an orphan empty Sheet is preferable to sending history to an uncertain destination. Apply the same rule to archive creation.

Create one spreadsheet per SmartLock installation, with a new archive file only at the growth threshold. A later account can own several such spreadsheets. Include both unit and installation identifiers now so future multi-device reporting needs no ID reinterpretation.

A backend means ongoing hosting/domain/database backup responsibilities. It is required for the selected experience, not for local door operation. If that operating commitment is unacceptable, the honest alternative is manually provisioned Apps Script with reduced UX, rather than secretly moving OAuth onto the ESP32.

## 5. Component diagram

```text
 LOCAL SECURITY DOMAIN                  HISTORY DOMAIN
 +--------------------------+           +--------------------------+
 | Touch / local browser    |           | Hosted Google consent UI |
 | Identity/Auth/Session/PIN |           +------------+-------------+
 +------------+-------------+                        | OAuth
              | local decision                       v
 +------------v-------------+           +--------------------------+
 | LockController -> GPIO22 |           | SmartLock HTTPS backend  |
 +--------------------------+           | SQL ledger + token vault |
              | audit event             +------------+-------------+
 +------------v-------------+                        | Sheets API
 | EventLog RAM -> SD CSV   |                        v
 | closed validated segment|           +--------------------------+
 +------------+-------------+           | Owner's Google Sheet     |
              |                         +--------------------------+
 +------------v-------------+                        ^
 | bounded CloudSync worker| -- HTTPS event batch -->|
 | no lock/auth/SD ownership| <-- exact commit ACK ---|
 +--------------------------+
```

The network worker receives an immutable byte buffer from the EventLog owner. Only the main EventLog owner performs segment validation, pinning, and deletion. No worker touches TFT, SPI/SD, lock state, or identity stores directly.

## 6. Data flow

1. Local authorization and LockController operate under existing local rules.
2. Audit captures safe event data, including identity snapshots for new-schema events. EventLog queues and persists it locally.
3. Only closed, fully CRC-validated SD segments qualify for upload. An event still only in RAM never qualifies.
4. CloudSync copies one segment into bounded worker storage and pins its exact ID while a request is in flight.
5. Backend validates the authenticated installation, link generation, segment digest, schema, rows, and IDs; durably stages them.
6. Backend writes/verifies deterministic Sheet ranges, then records their committed state.
7. ESP accepts only a matching complete ACK. The main owner revalidates the same segment bytes and retires it.

Cloud failure never changes an access result. SD failure must remain a visible logging fault rather than creating an alternative authorization path. The conceptual “SD first” is mandatory relative to **cloud**, but is not currently guaranteed relative to **unlock**. If strict pre-unlock durability is later required, use a distinct ACCESS_ATTEMPT intent followed by actual outcome, and explicitly decide whether storage failure denies otherwise authorized access. Do not log SUCCESS before actuation merely to satisfy ordering. This design recommends preserving current local availability and documenting the short RAM-to-SD power-loss window.

## 7. Authentication / OAuth design

### Owner pairing

1. An ACTIVE Owner with a fresh, locally validated Management authorization starts pairing through a POST on the permitted LAN interface. ADMIN/USER/GUEST cannot connect, replace, or disconnect Google. Preserve current Owner-only cloud configuration semantics; additionally enforce the intended interface policy explicitly.
2. ESP generates independent high-entropy device-poll and browser-claim secrets and a short-lived pairing transaction. It registers them with the fixed backend over verified TLS. Backend stores hashes, expiry (five minutes), and installation binding. Public pairing IDs confer no authority.
3. The authorized local page submits the one-use browser claim in an HTTPS form POST to a hosted page, opened by the user's Connect action. No secret goes in a URL. Backend exchanges it for a Secure, HttpOnly, SameSite cookie and consumes the claim.
4. Hosted page performs Google authorization-code flow with state binding and PKCE where supported by the chosen Google library. Validate exact callback, state, issuer/audience and account identity. Request offline access and only `openid`, `email`, and `drive.file`. Do not request full Drive access.
5. ESP polls outbound using its separate pairing proof. Hosted pages do not fetch `http://smartlock-...local`; this avoids HTTPS-to-local-HTTP mixed-content/private-network dependencies.
6. Local Management shows the proposed account and asks the Owner to confirm that account once. Final binding requires still-valid Owner authorization; if the five-minute Management authorization expired during Google consent, require a fresh Management login. Google login alone never proves local ownership.
7. Backend provisions the Sheet and returns an installation-scoped, revocable ingestion credential over the ESP's TLS channel. Persist connection state with verified two-slot NVS semantics. Failures leave the previous connection unchanged. Never expose this credential through status APIs.

Pairing completion must be idempotent across lost responses and reboot. Prefer an ESP-generated replacement ingestion secret registered over TLS, retained in a pending NVS slot until the backend confirms the same transaction; then activate that slot. Keep the old link usable until the new binding is committed on both sides, with a bounded recovery window, and do not silently fall back to it after explicit disconnect. Persist the pairing transaction's minimum recovery state without retaining the browser claim beyond expiry. A reused claim, wrong poll proof, expired local authorization or substituted installation must fail tests.

The existing local HTTP management channel retains its existing LAN threat boundary. Pairing does not upgrade that channel to HTTPS. Keep the one-use claim brief, require local authorization again at final binding, use no third-party scripts on the local page, and do not describe this as protection against an already compromised Owner browser or LAN interception.

Google supports creating spreadsheets with `drive.file`; that scope limits file access to files used with the app. V1 automatically creates a managed spreadsheet; selecting arbitrary existing spreadsheets is deferred to a later Google Picker flow. See [create API](https://developers.google.com/workspace/sheets/api/reference/rest/v4/spreadsheets/create) and [scope guidance](https://developers.google.com/workspace/sheets/api/scopes).

### Storage and lifecycle

| Location | Allowed stored state |
|---|---|
| ESP NVS | Fixed service configuration/version, installation ID, link generation, ingestion credential, non-secret Sheet/account display metadata |
| ESP SD | Existing events; versioned non-secret provenance if needed; no Google token |
| Local browser | Existing local credential under existing rules; pairing claim only briefly in memory; no new Google token in localStorage |
| Backend | OAuth client secret in secret manager; encrypted refresh tokens with separate encryption key; transient access tokens; ingestion-key hashes; delivery ledger and account/link metadata |
| Google | OAuth grant and owner's spreadsheets; no local SmartLock authority |

Refresh-token invalidation means “Reconnect Google,” not retrying a password or changing local identities. Reconnecting the same account can reuse its existing accessible managed Sheet. Changing account creates a new link generation and Sheet. Pause the old delivery worker first; never accept an old-generation ACK for new work. Explain that retained local history will be sent to the newly confirmed account; already retired history is not copied. Events already delivered to the previous account remain there. Delivery uniqueness is scoped to destination generation as well as installation/event, so an intentional destination change is not mistaken for a transport duplicate.

Backend token storage follows [Google's security guidance](https://developers.google.com/identity/protocols/oauth2/resources/best-practices). Plan a production OAuth project, consent branding, privacy policy, required verification, and Workspace administrator restrictions. External OAuth projects left in Testing can issue refresh tokens expiring after seven days for these scopes; staging success is not production readiness. See [OAuth lifecycle guidance](https://developers.google.com/identity/protocols/oauth2).

## 8. Google Sheet structure

Use three tabs, created and formatted automatically:

1. **ประวัติการเข้าใช้งาน** — ACCESS_GRANTED, ACCESS_DENIED, emergency unlock and emergency relock. Default readable view answers who, when, result, and emergency use.
2. **เหตุการณ์ระบบ** — normal UNLOCK/RELOCK details, enrollment/revoke, Management login, boot, meaningful network/recovery transitions, and history-loss observations.
3. **สถานะระบบ** — unit/installation, firmware last reported, last contact, last Sheet commit, queued records/segments, storage faults, cumulative loss counters, account/link state, timezone, and archive links. Include “as of” time; this is not live door status.

Each event is stored once in one event tab. The ordinary opening count counts ACCESS_GRANTED only, not both ACCESS_GRANTED and UNLOCK. Provide filter views for today, denied access, and emergency operations. Freeze headers and protect managed rows; let users analyze copies or separate personal tabs. Do not physically sort/insert/delete managed rows because row locations participate in recovery.

## 9. Exact proposed columns

Both event tabs share the following schema. A-H are visible by default; technical columns I-X are grouped/hidden. Stable machine names below are the contract; Thai headers are presentation metadata.

| Col | Machine name / Thai label | Value and rule |
|---|---|---|
| A | occurred_at_local / วันเวลา | Local display datetime; blank if unknown |
| B | time_quality / ความน่าเชื่อถือของเวลา | NTP or UPTIME; Thai rendering synchronized/unconfirmed |
| C | identity_name / ชื่อ ณ เวลาเกิดเหตุ | Historical verified actor or subject snapshot, as identified by identity_context; blank if unknown |
| D | identity_id / รหัสบุคคล | Safe D######; never hardware ID |
| E | role / บทบาท ณ เวลาเกิดเหตุ | Captured role; blank for legacy/unverified |
| F | action_label / เหตุการณ์ | Thai translation of stable action code |
| G | result / ผลลัพธ์ | Stable result with Thai display |
| H | source / ช่องทาง | LAN, RECOVERY_AP, PHYSICAL_ADMIN, UNKNOWN |
| I | event_id | Original stable ID, never regenerated |
| J | occurred_at_utc | Original UTC ISO value or empty |
| K | received_at_utc | Backend first durable receipt, clearly separate from occurrence |
| L | action | Original stable action enum |
| M | claimed_identity_id | Valid-format claim, not authenticated identity |
| N | verified_identity_id | Only credential-verified identity; revoked is not authorized |
| O | identity_context | ACTOR, SUBJECT, UNKNOWN; prevents enrollment target being called issuer |
| P | actor_identity_id | Actor if captured and verified; blank otherwise |
| Q | subject_identity_id | Enrolled/revoked identity if applicable |
| R | uptime_ms | Original uint32 value; not an absolute date |
| S | boot_id | Boot nonce from event ID; distinguishes uptime epochs |
| T | segment_id | Exact persisted basename |
| U | installation_id | Random installation epoch ID |
| V | unit_id | Stable non-secret hardware-derived identifier |
| W | firmware_version | Version captured at event creation; blank for historical rows without it |
| X | event_schema | Version of source event format |

No separate access_method column in V1: action plus source already expresses QR/browser versus physical PIN access without misleading redundancy. Keep network_mode in the backend technical receipt if useful; it is not ingress source and need not clutter Sheets. CRC is verified during ingest, not a user-facing column. Link generation, payload hashes and row assignments belong in the backend ledger.

Extend the existing CSV with a versioned header and bounded name/role/context/actor/subject/firmware fields for new events. Do not rewrite old files. Reuse existing name length limits; measure worst-case Thai UTF-8 and CSV escaping. Legacy ID-only history is displayed honestly, with blank name/role. Name snapshots are captured locally at event creation, so later revocation or rename cannot rewrite history. For denied access, a claimed ID never supplies a trusted name. A cryptographically verified but revoked identity can be identified with result DENIED.

Backend writes strings with `RAW`, not formula interpretation; also escape CSV correctly and protect any later CSV exports against spreadsheet formula injection. See [RAW value behavior](https://developers.google.com/workspace/sheets/api/reference/rest/v4/ValueInputOption).

## 10. Event-category mapping

| Existing action | Category / destination | Interpretation |
|---|---|---|
| ACCESS_GRANTED | Primary / door history | Local access authorized and software unlock path succeeded; not a door sensor observation |
| ACCESS_DENIED | Primary / door history | Separate verified identity from claimed identity; unknown stays unknown |
| UNLOCK, RELOCK | Diagnostic / system | Normal software lock sequence; preserve but do not double-count openings |
| EMERGENCY_UNLOCK_ADMIN_PIN | Primary/security / door history | Physical Admin PIN path; never assume the person was Owner |
| EMERGENCY_RELOCK | Primary/security / door history | Software timed relock following emergency unlock |
| DEVICE_ENROLLED | Security / system | Target identity; issuer only if explicitly captured by new schema |
| DEVICE_REVOKED | Security / system | Target revoked; snapshot persists after revocation |
| ADMIN_LOGIN | Security / system | Existing Management authentication event; not proof of physical PIN entry |
| BOOT | System | Reboot/boot occurred; no invented reset reason |
| NETWORK_CHANGED | System | Current Connected/Failed transition emission; not a complete Wi-Fi link-state history |
| RECOVERY | System/security | Recovery activation; do not infer request ingress from this |
| USER_ADDED | Legacy / system | Accept historical enum; do not restore old authorization model |

Proposed small additions, explicitly absent today: stable WIFI_CONNECTED/WIFI_LOST only for sustained transitions; HISTORY_LOSS observation derived from monotonic counters; optional FACTORY_RESET_REQUESTED if feasible before erasure. Do not log every reconnect attempt, DNS error, poll, upload chunk, or successful sync as a new SD event. Store cloud failures/status as aggregated diagnostics to avoid a logging feedback loop.

Loss observations use backend-generated IDs in a separate namespace keyed by installation and counter snapshot. They cannot be acknowledged as device events. Existing counters mean “at least/possibly this much history affected,” not a forensic exact count. PIN failure logging is not required for cloud V1; do not imply it already exists. A reset may erase its own final local record before upload; absence of a reset row does not prove no reset happened.

## 11. Sync algorithm

Use **one whole closed segment per batch**, at most 32 records and 8,192 bytes of original CSV. Byte limit wins over record count when snapshots increase row size. Keep the same segment ID and bytes on retries.

1. Require enabled connection, healthy SD, stable STA internet path, usable clock for TLS validation, and sufficient measured contiguous heap. Do not start while lock is unlocked, a local critical operation is underway, network is transitioning, or reset is pending.
2. Prefer an already closed segment. If only the current segment exists, seal only after its queue is drained and its oldest event has waited 60 seconds, or an Owner requests sync. Respect a failed seal; never upload the mutable current segment.
3. Validate and pin the exact closed segment. Build one immutable snapshot with its digest and ordered event IDs. Enqueue it to the lower-priority network worker; return immediately to the main loop.
4. POST raw CSV to a fixed versioned backend path, with bounded headers for installation ID, destination generation, segment ID, schema, and SHA-256 of the exact body. Use `Authorization: Bearer <ingestion-key>` and `Content-Type: text/csv; charset=utf-8`. Credential is only in the HTTPS header. No redirect following.
5. Backend validates everything before staging. A 202 RECEIVED response is **not** permission to delete. Backend durably processes its job even if ESP disconnects. Device retries the same POST after the advised delay; this also retrieves the completed receipt without a second ESP protocol.
6. A complete matching 200 COMMITTED receipt permits main-loop revalidation and exact segment retirement. Delete failure causes a safe resend, not loss. Process one completion per loop and schedule subsequent batches at least 15 seconds apart during backlog recovery.
7. Release the temporary in-flight pin on completion/error. Under strict pending retention, all remaining SD segments are implicitly unacknowledged and protected by policy, including after reboot. No fragile RAM-only delivery cursor is needed.

Do not scan every segment on every loop. Cache non-authoritative pending counts and update on append/seal/delete; perform bounded reconciliation after boot and on explicit diagnostics. No sync path may wait for Google in the lock/touch/HTTP loop.

## 12. ACK / idempotency protocol

Proposed full ACK (illustrative IDs):

```json
{
  "protocol": 1,
  "state": "COMMITTED",
  "installation_id": "32-hex-installation-id",
  "link_generation": 2,
  "segment_id": "exact-segment-basename",
  "segment_sha256": "64-hex-digest",
  "event_ids": ["original-40-hex-event-id"]
}
```

Accept only verified TLS, HTTP 200, supported protocol/state, exact installation/link/segment/digest, and exactly the ordered IDs from the pinned segment. Maximum response 4,096 bytes; reject duplicate keys, truncated JSON, oversized fields, extra IDs, duplicate IDs, unsupported schema, or mismatches. Never trust HTTP 200 alone. There is no partial-delete operation. Partial completion remains RECEIVED/PENDING until every event has been verified in Sheets.

Backend tables need only connections, batches, deliveries and row counters/jobs. Unique delivery key: `(installation_id, link_generation, event_id)`. Store canonical event hash, target spreadsheet/tab/row and state. Same ID with different canonical data is a conflict, never overwrite or acknowledge it. Batch identity also binds the complete body digest. Authenticated requests cannot select arbitrary accounts or Sheets; backend derives destination from the connection.

**Critical crash window:** a Sheet append can succeed while its HTTP response is lost. A database uniqueness constraint alone does not prevent a second append. Therefore do not use blind `values.append`.

Before writing, reserve exact rows in a durable transaction under one writer lock per spreadsheet. Retry writes to those same ranges using `values.update`/bounded batch update with RAW values. Read back all expected canonical cells/IDs and compare before marking deliveries COMMITTED in SQL. Only then issue the ACK. Fixed-range writing is supported by the [Sheets values update API](https://developers.google.com/workspace/sheets/api/reference/rest/v4/spreadsheets.values/update); row reservations and recovery are our application design, not an exactly-once guarantee supplied by Google.

Persist the chosen projection/schema version with each reservation. A later translation or formatting release must not change the expected bytes for an already reserved event. The Sheet stores all fields needed to reconstruct its canonical event hash; firmware CRC and backend delivery hash have different purposes. Preserve source-only legacy fields in the durable receipt if they are included in that hash.

Recovery cases:

- Crash before write: use saved reservation and write it.
- Write succeeded, response or SQL commit lost: inspect reserved cells, verify identical content, then commit; never allocate new rows.
- Some ranges persisted: verify existing ranges, write missing reserved ranges, ACK only when all match.
- ACK lost or ESP rebooted: original segment is resent and returns the same complete receipt.
- ESP deleted after ACK then rebooted: backend retains its ledger; no local replay is necessary.
- Cell/range contains unexpected content: pause that destination and reconcile; never overwrite unrelated user rows.

This contract assumes managed rows are not arbitrarily moved by human edits. Protect them, use filter views, check metadata/header and target rows, and surface modification conflicts. Google and SQL do not share a transaction. Concurrent owner edits can defeat absolute exactly-once claims; document this boundary. Ledger backups are essential. If the ledger is lost, stop writes and rebuild ID/hash/row mappings from Sheets before accepting more batches. Do not expire dedup entries while their Sheet exists or devices might retry them.

## 13. Local retention policy

| Option | Benefit | Cost | Decision |
|---|---|---|---|
| 1. Retire exact fully acknowledged segment | Uses existing delete API; no ACK ledger on ESP; maximum offline capacity | Uploaded history is no longer browsable locally | **Recommended V1** |
| 2. Keep a short acknowledged local history | Recent local viewing without internet | Needs persistent ACK metadata and recovery rules | Defer; not needed for long-term Google history |
| 3. Current bounded rotation plus cursor | Small behavioral change | Can erase unsent records; cursor can point into vanished history | Reject for enabled reliable-sync mode |

While cloud is enabled, SD contains pending delivery segments only. A complete ACK makes exactly one segment eligible for retirement. Never remove partially acknowledged, corrupt, or uncertain segments as successful cleanup. Temporary request pins remain useful, but **all unacknowledged segments** need retention protection across reboot, not just the one pin.

At 64 segments, if none can be acknowledged, stop accepting additional records once existing free capacity is exhausted. Do not delete old unsent records or newly persisted records. Continue access locally and expose **new events not recorded due to full queue** separately from prior overwritten-history counters. This cannot preserve the unrecorded new events; the owner must see that history is incomplete. At full size, approximately 2,048 records fit; normal access currently uses three records, so roughly 682 complete openings is an upper-bound illustration, not a guaranteed number of days. Partial segments and other events lower it.

Persist a loss-observed marker on first overflow and bounded periodic aggregate checkpoints thereafter, rather than an NVS write per hostile denied request. Report an explicit lower bound/uncertainty after sudden power loss; never claim an exact durable count if increments were only in RAM. Existing loss counters must not be reset or conflated with acknowledged cleanup. Test NVS failure and counter saturation.

When cloud has never been enabled, preserve existing bounded recent-history policy. On disconnect, leave pending history protected until the Owner explicitly chooses to discard it or reconnect; do not silently resume destructive rotation. V1 need not provide a discard button. Factory Reset remains the existing explicit exception that erases local history. These are intentional retention semantics to implement and test, not behavior already present in SL-08.

Persist this protected-history policy independently of the enabled flag so reboot or disconnect cannot accidentally revert to eviction. Status/export must continue exposing oldest/newest retained segment IDs, pending counts, existing overwritten counters and new unrecorded-event evidence. No cloud cursor may imply that missing segments were uploaded.

## 14. Retry / backoff behavior

ESP transient failures: 15, 30, 60, 120, 240, 480, then 900 seconds, with bounded random jitter. Use wrap-safe elapsed/deadline arithmetic; no busy waits. Honor a bounded backend Retry-After up to one hour. Reset backoff after a verified commit, not merely a TCP connection. On boot add startup jitter; do not persist each retry counter to NVS.

Use one connection, five-second connect limit, eight-second handshake/read component limits, and a **20-second total worker transaction deadline** including DNS/connect/TLS/body. Audit the actual framework calls: a supervisor flag cannot interrupt a blocking call that ignores timeout. Accept this gate only after fault-injection shows each layer returns within budget; otherwise choose a bounded transport implementation before deployment. Never force-delete a task holding shared locks.

Backend retries Google 429/5xx with jitter and per-account/project rate limits. Invalid authorization pauses Google delivery and requests reconnect; malformed event data is quarantined, not retried at high frequency. Device can poll connection status at a slow interval while paused; do not repeatedly upload the same batch every few seconds during a known OAuth failure.

## 15. Offline behavior

Access, enrollment, Management, PIN, lock timing and network recovery remain local. Lack of internet, DNS, NTP, backend or Google does not alter their authorization. Cloud worker simply defers.

New events remain on SD until exact acknowledgement, subject to the finite-capacity admission/loss policy above. RAM queue loss on abrupt power failure remains possible. On reconnection, upload oldest valid closed segments first at the bounded catch-up rate. A corrupt segment is retained and visibly quarantined; allow subsequent validated segments to sync through explicit named selection so one damaged file does not block all later history. Do not silently drop a corrupt row and ACK the rest as a complete segment.

The backend must bound its own staging too: one pending segment per installation is sufficient because ESP sends one at a time. Google outage is not permission to ACK staged-but-unwritten events. Expired staging can be rebuilt from the still-retained SD segment; keep durable reservations and hashes needed to resolve uncertain Google writes.

## 16. Security / privacy model

Never upload browser credentials, credential hashes/verifiers/salts, ADMIN PIN or its verifier, Wi-Fi/AP passwords, raw enrollment/Management/Access tokens, Google tokens, private keys, or any local authorization database. The dedicated ingestion credential is transmitted only as required for authentication, redacted from request/exception logs, and never written to event rows.

Current EventLog fields are constrained IDs, enums and times; they are suitable non-secret inputs. CRC is not a credential. New display-name snapshots are personal data: bound and escape them, store only the name needed for historical attribution, and do not upload the entire identity list. Avoid client IP/MAC, raw headers, URL queries, request bodies, and arbitrary error strings in events.

Backend can learn who used the door and when. State that in consent/privacy information, restrict operator access, encrypt stored tokens/backups, redact application logs, and retain staged event content only until delivery/recovery needs expire. Keep the compact dedup/row ledger for the lifetime of managed history. An ingestion key permits only its own installation's history/status operations, never Google account management or local door commands. Rate-limit it and support revocation/rotation.

Require HTTPS with hostname and chain verification. Reuse WiFiClientSecure already linked; use a small explicitly selected backend trust anchor set with overlap for planned CA rotation, not insecure mode or a leaf fingerprint that expires quickly. Do not automatically trust a downloaded CA. Keep a valid clock prerequisite; NTP unavailable means sync waits. Backend uses maintained platform trust for Google. Trust-anchor changes require the normal approved firmware release process, not reintroduced browser OTA.

## 17. Factory Reset behavior

Existing reset clears the SmartLock SD tree and `sl-cloud`/`sl-audit` namespaces. Extend its clear-list to any new integration namespace; preserve touch calibration according to existing policy. Reset removes local ingestion key, pairing transactions, connection metadata, pending events and installation ID. Unit ID remains derivable; next Setup/integration gets a new random installation ID. D000001 from a new installation must never collide logically with an earlier D000001.

Never delete Google Sheets during Factory Reset. Existing committed history remains in the owner's account. Do not delay reset waiting for Google. If online, a best-effort disconnect may be attempted only within an already bounded background path; do not make it a prerequisite. Offline reset cannot guarantee remote OAuth revocation. Backend shows the old installation as stale, expires inactive links under a documented policy, and provides hosted account disconnect; owner can also revoke the app in Google.

A previously staged batch may finish in the old Sheet after an offline reset. It remains old-installation history and cannot authorize the new device. Do not promise a FACTORY_RESET success event reaches Google when reset itself erases the queue.

## 18. Resource / flash considerations

Current optional CloudSync already links HTTPClient, WiFiClientSecure, certificate material and an 8,192-byte worker stack. It builds JSON bodies up to roughly 16 KB from an 8 KB CSV buffer, with another payload possible during ACK comparison. Replace that expansion with one raw CSV snapshot and bounded ACK parsing; do not add a Google SDK on ESP.

Planning budget, **not measured build results**: target net firmware growth below 25 KB, retaining roughly 28 KB of the present headroom; stop and review if the image approaches the slot limit. Do not change partitions. Target one 8,193-byte segment buffer, <=4,096-byte response buffer, roughly 1.3 KB ID list if retained, small metadata, and the existing worker stack. TLS can require tens of KB of heap and contiguous allocations; the exact peak depends on framework/server chain and must be measured.

The previous report observed minimum heap around 102,784 bytes after HTTP tests, but did not certify new cloud TLS workloads. Name snapshots enlarge the 16-entry RAM queue and reduce records per segment under the byte cap. Measure static RAM, largest free block, free/minimum heap, TLS handshake peak and recovery over repeated cycles. Do not hold duplicate payload Strings or allocate whole history. Release the TLS connection between batches.

Google's documented default Sheets quotas are 300 read and 300 write requests/minute/project, with 60 of each/minute/user/project; use batched writes/readbacks and stay far below them. API requests can time out after 180 seconds, so backend staging decouples that from the 20-second ESP transaction. See [Sheets limits](https://developers.google.com/workspace/sheets/api/limits).

Archive at 50,000 total event rows per spreadsheet, well before product cell limits; create a successor automatically and keep the previous file intact. Route each reserved delivery to its original archive generation forever. Store archive links on the status tab. Current Drive help lists up to 20 million cells for Sheets; this design does not depend on reaching that limit, and limits should be rechecked at release. See [Drive file limits](https://support.google.com/drive/answer/37603?hl=en).

## 19. Management UI design

Owner path: **ระบบ -> Google Sheets**.

```text
Google Sheets
สถานะ: ยังไม่ได้เชื่อมต่อ
[ เชื่อมต่อ Google ]

บัญชี Google: owner@example.com
ชีต: SmartLock ...
สถานะ: เชื่อมต่อแล้ว / รออินเทอร์เน็ต / ต้องเชื่อมต่อใหม่
ซิงก์สำเร็จล่าสุด: ...
ข้อมูลรอส่ง: ... เหตุการณ์ / ... ส่วน
ประวัติที่สูญหายหรือบันทึกไม่ได้: ...
[ ซิงก์ตอนนี้ ] [ เปิด Google Sheets ] [ ยกเลิกการเชื่อมต่อ ]
```

“Sync now” schedules work; it never blocks the HTTP handler or overrides backoff for a known persistent failure. “Open” uses a validated Google Sheets URL with no credential. “Disconnect” explains that the Sheet stays, pending local history stays, and Google grant revocation may require internet. Account change is disconnect/reconnect with explicit destination confirmation, not an extra complex settings page.

Owner-only mutations; ADMIN may see existing permitted non-sensitive status but not account-changing controls. Status displays last contact time and stale state honestly. Add clear Thai states for storage full, damaged history, missing Sheet, and authorization revoked. No pasted script URL, spreadsheet ID, token fields, column editor or API-key UI.

Time: retain UTC in technical data; use IANA `Asia/Bangkok` as initial display timezone, configurable in hosted connection settings, not firmware hardcoded UTC+7. UPTIME-only rows show **เวลาไม่ยืนยัน**, blank occurrence datetime, uptime and separately labeled received time. Current TimeManager considers NTP freshness for about 24 hours. Never infer precise historical time across reboot or millis wrap; V1 performs no speculative backdating. Sort unknown-time records by receipt/boot/sequence with an uncertainty label rather than mixing them into an exact “today” claim.

## 20. Failure handling

| Failure | Cloud response / recovery | Local effect |
|---|---|---|
| No internet / DNS failure | Bounded timeout, backoff, retain segments | Access unaffected |
| TLS chain/hostname/time failure | Refuse connection; show diagnostic; never disable verification | Access unaffected |
| Backend/Apps Script unavailable | Selected backend retries; Apps Script is not a V1 dependency | Access unaffected |
| Google 429/5xx | Backend retains job; jittered retry; ESP receives pending | No segment deletion |
| OAuth access token expired | Backend refreshes using protected refresh token | Access unaffected |
| Grant revoked / refresh invalid | Pause delivery; Owner reconnect | Queue retained |
| Spreadsheet deleted / permissions removed | Pause; offer reconnect/create replacement explicitly | No silent ACK or history reconstruction claim |
| Spreadsheet moved | Continue by stable file ID if access remains; otherwise permission failure | Access unaffected |
| Malformed / oversized response | Reject entirely; retry safely | Queue retained |
| HTTP success with only some events stored | No complete ACK; finish reserved rows and readback | Entire segment retained |
| ESP reboot during upload | Lose temporary worker/pin; discover retained segment and resend | Existing normal boot starts locked |
| SD row corrupt | Quarantine and report; sync other valid segments by name | Logging incomplete, authorization unchanged |
| SD full / prolonged outage | Protect unsent records; count/report unrecorded new events | Access continues |
| Worker resource exhaustion | Cancel/defer bounded cloud work; never alter lock state | Must pass stress gate before release |
| Backend process crash | Durable jobs resume; no local dependency | Access unaffected |
| ESP firmware panic | MCU may reboot; cannot promise uninterrupted access | Existing fail-closed boot applies; must be prevented by tests |
| User changes managed rows | Conflict detection and recovery pause | Access unaffected |
| Counter persistence fails | Surface storage fault and uncertain loss count | Never report loss-free history |

## 21. Testing strategy

No tests or services are run as part of this design. Future evidence must distinguish the following levels.

| Level | Required tests / acceptance |
|---|---|
| HOST TEST | Production EventLog parsing of both old headers and new schema; CRC corruption; exact ID preservation; Thai names and escaping; byte/record limits; whole ACK validation; wrong installation/link/hash/IDs; partial ACK; millis wrap; persistent retention across reboot; full-capacity admission/loss accounting; NVS failure and saturation |
| MOCK RECEIVER TEST | Duplicate concurrent batches; lost ACK; Google-write-success/response-loss; SQL crash before/after reservation/commit; partial ranges; conflicting same ID; destination change; oversized/trickle response; timeout; quota simulation; ledger restoration; user-edited row conflict |
| LIVE ESP32 TEST | Staging endpoint only; TLS valid/invalid chain/hostname; DNS/no-internet; repeated handshake failures; free/min heap/largest block; loop latency; touch/HTTP responsiveness; timer/relock scheduling; SD queue/reboot persistence; no secret Serial output; pin release and storage faults using safe fixtures |
| REAL GOOGLE TEST | Dedicated staging account/Sheet; actual OAuth consent/create; RAW Thai/formula-like names; exact readback; repeated/lost-response uploads; token revoke/reconnect; move/delete file; archive creation; row protection/filter views; no duplicate event IDs; no deletion before verified Sheet persistence |
| PHYSICAL USER TEST | One final normal and emergency unlock/relock observation, with corresponding local and Google sequence; only this step proves physical behavior. Initial Google consent is a real user/account interaction unless a preauthorized staging account is provided |

Use fake time instead of waiting days. Use production methods wherever practical, storage fixtures for retention and failure injection, and a mock Google adapter for crash boundaries. Never use a test that merely duplicates parser logic as the sole acceptance evidence.

Final integrated gate includes Owner access, revoked denial, enrollment, Management lifetime, PIN/network route authorization, GPIO ownership, normal/emergency timed relock, SD retention, no secret leakage, and normal reboot. Cloud stalled at each network stage must not monopolize the main loop. Compare maximum loop/relock delay against baseline and set a measured acceptance bound before release. Run a multi-hour cloud failure/recovery soak, not only one successful upload. Physical PASS cannot be inferred from Serial or Sheet rows.

## 22. Migration from existing CloudSync prototype

| Existing component | Decision |
|---|---|
| EventLog IDs, CRC, closed-segment APIs, exact deletion | Reuse; extend schema compatibly, add strict pending-retention mode and named upload selection |
| CloudSync separate FreeRTOS network worker/queues | Reuse ownership boundary; add total deadlines, resource and local-operation scheduling gates |
| Two-slot verified NVS cloud config | Reuse persistence pattern with explicit version/migration; preserve disabled state |
| CloudProtocol exact ACK principle | Reuse principle; replace provider-specific envelope/parser with versioned digest/link-bound receipt |
| GoogleRoots / Apps Script redirect handling | Replace with selected backend trust anchors and no redirects after endpoint migration |
| Script URL/shared-secret Management form | Replace with Owner pairing/status; no credential migration to Google |
| `cloud/apps_script/Code.gs` | Keep clearly archived as prototype/test history; do not deploy for new production design |

The current script accepts exactly nine old event fields and omits claimed identity/access source; its action whitelist also lacks emergency unlock/relock. Current firmware serializes the newer attribution fields, so enabling this script is not a valid end-to-end upgrade. It also scans all stored rows on each batch and enforces 50,000 rows without archive provisioning. Its script lock and readback are useful prototype ideas, not a substitute for the new contract.

Current firmware cloud configuration is Owner-authorized; status uses Management authorization. Cloud transport currently restricts the URL to an Apps Script deployment and follows a restricted Google content redirect. None of this constitutes Google account connection. Current worker success/retry counters are RAM status, not durable account/link state.

Do not auto-enable cloud on firmware update, reinterpret existing secrets, rewrite historical CSVs, or change local identity authority. Initialize new installation metadata without changing Owner/PIN/Wi-Fi. Legacy rows acquire installation scope at migration but retain unknown historical name/firmware fields. Existing loss counters remain visible. Introduce parser/schema support before writing any new rows.

## 23. Implementation phases

1. **Protocol and backend proof, no production board:** fixture-based receiver/ledger, deterministic Sheet adapter, OAuth pairing design tests, crash/lost-ACK tests. Establish hosting, secrets, domain, backup and consent-project ownership. Settle the explicit SD-before-unlock interpretation and strict-full-queue tradeoff as product acceptance criteria, not hidden implementation choices.
2. **Local metadata and retention:** versioned snapshots, installation ID, legacy compatibility, protected pending history, bounded loss reporting, named segment selection. Host tests and resource estimate; no Google credentials on ESP.
3. **Compact device integration and Thai UI:** reuse worker, raw CSV transport, exact ACK, total deadline, heap gates, Owner-only pairing/disconnect, no main-loop TLS. Build/size/host/browser regression before any flash approval.
4. **Staging end-to-end:** mock failures, real staging Google consent/Sheet, then approved non-destructive board deployment and soak. Preserve production authority/configuration. Prove recovery and no duplicates, not just happy-path row creation.
5. **Production acceptance:** publish proper OAuth configuration, operational backup/recovery, final physical check, enable only through Owner action. Track Sheet/archive status and cloud resource metrics; no remote-control expansion.

SOL owns security decisions, review and acceptance. LUNA can later implement bounded schema/parser/UI/fixture tasks under SOL review. This document itself does not dispatch implementation or grant flash permission.

## 24. Risks / tradeoffs

- **Finite offline history:** strict no-delete-unsent means new events become unrecordable at capacity. Keeping all history forever requires more storage or a different product requirement. No ACK protocol solves this.
- **Local durability gap:** queued events may be lost on immediate power failure; current Access is not SD-write-ahead. Cloud design cannot retroactively fix that.
- **Resource margin:** 53,504 bytes is modest. Existing TLS linkage helps, but actual flash/heap cost remains unmeasured until implementation.
- **Backend operations:** someone must maintain HTTPS, secrets, database backups, OAuth production setup and recovery. A backend outage affects history latency, not local authorization.
- **Editable spreadsheet:** fixed-row idempotency requires managed ranges and conflict handling. Google Sheets is not a tamper-proof audit store, and an Owner can intentionally delete history.
- **Time uncertainty:** no NTP means no exact occurrence date. Receipt time is useful but not a substitute.
- **Historical attribution:** existing rows lack names/roles; these cannot reliably be reconstructed after identities change.
- **Account/reset boundaries:** offline reset cannot revoke remote grants immediately. Account changes need explicit historical-data destination disclosure.
- **TLS task isolation limits:** memory corruption can still reset an ESP32. Failure containment must be tested rather than claimed from task separation alone.
- **Growth:** archive at a conservative row threshold. No million-event database, automatic history deletion, or analytics platform is needed now.

## 25. Exact recommendation for SOL to implement next

Start with **Phase 1 only: a local backend/protocol test implementation using recorded non-secret EventLog fixtures and a fake Sheets adapter**. Prove deterministic row reservation, lost Google response recovery, exact ACK validation, partial-batch handling, destination-generation binding and ledger recovery before touching firmware.

Then implement the small EventLog metadata/retention extension and compact worker integration in reviewed increments. Keep existing authorization, GPIO22 ownership, PIN, Wi-Fi and partition layout intact. Do not enable the old Apps Script prototype as a shortcut. Do not accept a cloud test that reports success merely because HTTP returned 200.

The release acceptance contract is: local access remains independent; only durable validated events upload; identical retries do not create duplicate rows under the managed-sheet contract; only exact verified Sheets commits authorize local cleanup; offline/full/corrupt history is visible; secrets never become log data; resource and physical evidence are reported honestly.
