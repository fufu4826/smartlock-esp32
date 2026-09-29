HISTORICAL: Google Sheets is removed from the active product. This document records an earlier design or test checkpoint and is not current implementation authority.

# SmartLock Google Sheets — free-only cloud architecture

> **HISTORICAL / SUPERSEDED (2026-09-27):** This design is retained as historical context. The approved current architecture is [Google Sheets one-click](GOOGLE_SHEETS_ONE_CLICK_ARCHITECTURE.md). This document does not describe the current approved design or establish deployment acceptance.

Architecture decision: **A — Cloudflare Workers Free + D1 Free + Google APIs**, on a `workers.dev` HTTPS origin. Design reviewed against official documentation on **2026-09-27**.

**DESIGN ONLY.** This document does not authorize deployment, account changes, firmware changes, or hardware access. No production source was changed, no service was deployed, and no live Google acceptance is claimed.

## 1. Executive summary

Replace the conventional Node/PostgreSQL hosting assumption with one Cloudflare Worker, one D1 database, and one optional recovery Cron trigger, all on the Free plan. Use the included `workers.dev` hostname and managed HTTPS. Keep Google OAuth and Sheets/Drive calls in the backend. Keep the ESP32 protocol and local security boundary unchanged.

This is a practical **zero-billing architecture for the current personal SmartLock installation**, not a promise of unlimited free storage, uninterrupted cloud availability, or unchanged provider terms forever. Free quota exhaustion pauses history delivery; it cannot authorize a door action or force a paid upgrade. Never attach a billing account or payment card to enable this integration. If a provider later requires either, stop cloud synchronization and reassess a free replacement.

The recommendation is supported by current documented free services. Deployment acceptance still requires three concrete proofs: the actual account accepts the exact `workers.dev` OAuth callback without a purchased domain; the Worker fits the Free CPU budget; and its responses satisfy the existing ESP32 HTTP/TLS parser. These are unperformed acceptance gates, not claims of deployed success. Public distribution requiring verified branding/domain ownership is outside the current personal-use recommendation.

Most protocol, event projection, pairing security, and Google row semantics survive. PostgreSQL transaction callbacks, advisory locks, Node HTTPS serving, and the continuous background loop do not. Replace those infrastructure assumptions with atomic D1 operations and a resumable delivery state machine. Do not rewrite EventLog or add a Cloudflare SDK to the ESP32.

## 2. Hard zero-cost requirements

- Production subscriptions, required storage, domain, certificates, OAuth, scheduling, and monitoring must total **0 THB / 0 USD**.
- No card, linked Google billing account, paid plan, trial, paid quota increase, paid queue, or automatic overage path.
- Select an explicitly Free Cloudflare account/plan. Do not rely on a spending alert or a paid-plan budget cap as a zero-cost guarantee.
- Use a Google project with billing **unlinked**. Do not activate the Google Cloud free-credit trial. Enable only the required standard APIs.
- Quota or account restrictions must produce no valid COMMITTED receipt. Preserve the exact pending segment under existing protection rules.
- Finite storage remains finite. Once protected SD capacity is full, existing pending events remain; subsequent logging can be refused with loss evidence while local access continues.
- No payment prompt in Management. Recovery choices are wait, reduce usage safely, reconnect, or reassess a free provider—not upgrade.

The cost guarantee is a configuration and operating rule under the dated provider terms. No architect can guarantee an external provider's perpetual pricing or availability. Cloud failure is explicitly permitted; paid fallback is not.

## 3. Current implementation summary

Repository inspected: `C:\ESP\esp32_035_lock_touch_test`. Current HEAD: `0ce8c663769c456325d19b2b3439297d8ebb9ca6`; implementation commit: `9217f0c75cdb144a07ac2d451f9150e886f621da`.

Authority: current `backend/src/{protocol,projection,database,ledger,service,google,pairing,archives,vault,production}.ts`, SQL migrations, `backend/DEPLOYMENT.md`, and the Phase 01/complete implementation reports. The earlier architecture document describes an older foundation; it does not override schema 3 and protected pending history now implemented.

| Area | Current implemented behavior to preserve |
|---|---|
| Events | Schema 3 identity snapshots and source attribution; legacy CSV accepted; immutable CRC-validated segments, at most 8,192 bytes and 32 events |
| Identity | Random persistent installation ID, separate from hardware unit ID and D###### browser identity |
| Retention | Mode B protects pending segments, including after disconnect/reboot; explicit overwritten/unrecorded counters |
| Sender | Lower-priority immutable-job TLS worker; no direct SD/TFT/GPIO/auth access; bounded transaction |
| Cleanup | Exact seven-field COMMITTED receipt and current generation required before selected-segment retirement |
| Backend | PostgreSQL/PGlite, transactional row reservations, cross-process advisory locks, fixed RAW Sheet ranges and complete readback |
| Pairing | Local Owner initiation, separate claim/poll proofs, Google state/PKCE/nonce, explicit binding confirmation |
| Google | Backend-only encrypted refresh token; scopes `openid`, `email`, `drive.file`; marker-based provisioning and approximately 50,000-event archives |
| UI | Thai Management connect/confirm/status/sync-now/open/disconnect; no pasted API credentials or Sheet IDs |

The complete implementation report records 83 backend cases and 21 integrated host/browser commands passing in the prior implementation. Those results were **not rerun here** and do not certify D1. Real Google setup remained blocked. The reported full-cloud resource build is **1,271,792 bytes**, leaving **38,928 bytes** in the **1,310,720-byte** slot. The smaller default deployed build has cloud origin disabled; its size cannot establish active-cloud capacity.

Existing report/evidence modifications were left untouched. No COM6 access, log mutation, browser storage change, or production build was performed for this design.

## 4. Official free-tier research

All links below were checked on **2026-09-27**. Provider facts are distinguished from the proposed engineering design in later sections.

### Cloudflare

- Workers advertises free entry without a credit card. Workers Free includes 100,000 daily requests and a 10 ms CPU allowance per invocation. Paid Workers is a separate subscription; this design does not select it. [Workers product](https://www.cloudflare.com/products/workers/), [pricing](https://developers.cloudflare.com/workers/platform/pricing/).
- D1 Free includes 5 million rows read/day, 100,000 rows written/day, and 5 GB account storage. Daily exhaustion causes query errors until reset at 00:00 UTC; full storage requires safe space recovery. Index work also consumes quota. No paid overage applies to this chosen Free plan. [D1 pricing](https://developers.cloudflare.com/d1/platform/pricing/).
- A Free D1 database is capped at 500 MB; the account permits 10 databases. There are 50 D1 queries per Worker invocation, 100 bound parameters per statement, and seven days of Time Travel. Do not mistake the 5 GB account allowance for one database's limit. [D1 limits](https://developers.cloudflare.com/d1/platform/limits/).
- Workers Free has 128 MB isolate memory, 50 external subrequests/invocation, six simultaneous outgoing connections, and five Cron triggers/account. Free Cron also has a 10 ms CPU budget. Network waiting is distinct from CPU. Request exhaustion returns an error; CPU exhaustion can terminate execution. `waitUntil` is time-limited and is not a durable queue. [Workers limits](https://developers.cloudflare.com/workers/platform/limits/).
- Worker secret bindings, scheduled handlers, and outbound HTTPS fetch are supported platform facilities. Use ordinary Worker secrets, not a separately priced secret-management product. Google OAuth and REST endpoints are compatible with this outbound model. [Secrets](https://developers.cloudflare.com/workers/configuration/secrets/), [Cron](https://developers.cloudflare.com/workers/configuration/cron-triggers/), [fetch](https://developers.cloudflare.com/workers/runtime-apis/fetch/).
- `workers.dev` provides the hosted endpoint without buying a domain. Cloudflare describes it as intended for personal/hobby projects and recommends custom domains for business-critical production. Accept that limitation for optional personal history delivery, not for door availability. [workers.dev](https://developers.cloudflare.com/workers/configuration/routing/workers-dev/).

### Google: do not repeat the obsolete claim that all future API usage is unconditionally free

Standard Sheets API use currently has no additional charge. Published quotas are 300 reads and 300 writes/minute/project, each additionally limited to 60/minute/user/project. Excess currently yields 429. Its pricing page explicitly announces planned chargeable over-quota use later in 2026. [Sheets limits](https://developers.google.com/workspace/sheets/api/limits).

Google's new model says the scaled paid option requires agreement and enabled billing; standard quotas remain the path without that upgrade. Google project creation describes billing as optional depending on services. Thus the selected standard-use configuration needs no billing account/card; **do not request the paid quota option**. Account-specific API enablement must still be verified before deployment. [Workspace API tiering](https://developers.google.com/workspace/tools-safety), [project creation](https://developers.google.com/workspace/guides/create-project).

Drive's new-project model includes 1,000,000 quota units/minute/project and 325,000/minute/user/project, with a 400-million-unit daily standard threshold and a separate egress limit. Calls have different unit weights. This design uses only small metadata/create operations, never bulk downloads, and remains far below standard thresholds. Drive also announces future paid excess usage; no billing is to be enabled. [Drive limits](https://developers.google.com/workspace/drive/api/guides/limits).

`drive.file` is non-sensitive and limits file access. Personal-use apps below the documented user limit can qualify for exemption from verification; that is not a blanket exemption for public distribution. External Testing refresh tokens normally expire after seven days when requesting these file scopes. [Drive scopes](https://developers.google.com/workspace/drive/api/guides/api-specific-auth), [verification exceptions](https://support.google.com/cloud/answer/13464323?hl=en), [OAuth token lifecycle](https://developers.google.com/identity/protocols/oauth2).

### Alternatives

Supabase Free provides PostgreSQL, 500 MB/database, two active projects, 5 GB egress, and 500,000 included Edge Function invocations per billing period; projects may pause after a week of inactivity. Its Free plan is not billed for excess; restrictions apply instead. Edge Functions have 256 MB memory, 2 seconds CPU/request, 150 seconds Free wall duration, and secret support. Scheduled calls can use `pg_cron`/`pg_net` and Vault. [Pricing](https://supabase.com/pricing), [cost control](https://supabase.com/docs/guides/platform/cost-control), [runtime limits](https://supabase.com/docs/guides/functions/limits), [scheduling](https://supabase.com/docs/guides/functions/schedule-functions).

Supabase is a credible alternative with more PostgreSQL reuse. Its reviewed official pages establish a Free plan but did not provide an explicit no-card onboarding statement comparable to Cloudflare's; account onboarding was not attempted. Treat that criterion as **unverified**, not silently certified. Default hosted HTTPS and Edge REST functions support callback/API implementation; arbitrary Node server/advisory-lock lifetime still needs adaptation. [Edge quickstart](https://supabase.com/docs/guides/functions/quickstart).

Apps Script has a free consumer scripting environment. Consumer quotas include six minutes/execution, 90 minutes/day trigger runtime, 20,000 URL Fetch calls/day, and 500 KB/property store; quota breaches throw exceptions. Web apps offer Google-hosted HTTPS and owner/user execution modes. Content Service redirects clients to a separate Google content URL. These are material differences from our existing authenticated, non-redirecting API. [Google's scripting overview](https://workspace.google.com/blog/developers-practitioners/data-processing-just-got-easier-apps-scripts-new-v8-runtime), [quotas](https://developers.google.com/apps-script/guides/services/quotas), [web apps](https://developers.google.com/apps-script/guides/web), [Content Service](https://developers.google.com/apps-script/guides/content).

Firebase Functions requires Blaze for deployment, even though some usage is included. It fails the no-billing rule and is excluded without further runtime design. [Firebase deployment requirements](https://firebase.google.com/docs/functions/get-started).

## 5. Candidate comparison

| Candidate | Fit and limitations | Decision |
|---|---|---|
| A. Workers Free + D1 + workers.dev | Explicit no-card entry; hard Free limits; durable transactional database; built-in HTTPS/secrets/Cron; moderate SQL/runtime migration; tight CPU gate | **Select for this personal installation** |
| B. Supabase Free + Edge Functions + hosted PostgreSQL | Better SQL reuse and CPU allowance; secrets/HTTPS/scheduling available; inactivity pause and no Free automatic database backup; no-card onboarding not explicitly established in this review | Viable fallback to investigate, not certified/selected |
| C. Owner-deployed Apps Script | No paid hosting/billing needed for consumer service; automatic Sheet creation possible after script authorization; durable dedup would need a significant Sheet-backed ledger redesign; script deployment, execution identity, redirects and receiver authentication differ | Reject for current no-URL-paste UX and minimal-migration requirement, not because idempotency is intrinsically impossible |
| D. Existing home computer as backend | Can reuse Node/PostgreSQL, but depends on an always-on computer, power and reachable stable HTTPS callback. A temporary tunnel is not a stable production OAuth origin | Reject as a complete autonomous cloud replacement |
| Screened out: Firebase/Cloud Run paid-capable deployment | Billing-enabled runtime violates hard requirement regardless of nominal free usage | Reject |

Apps Script secrets could live in private script properties, but this is not a relational ledger or equivalent encrypted token vault. It has scheduled triggers and outbound URL Fetch, yet no drop-in preservation of current header authentication/HTTP framing. Supabase's monthly invocation allowance and usage restrictions would also need account confirmation before selecting it; no alternative is accepted on unverified free-tier assumptions.

## 6. Selected architecture

Use one account-owned, permanently named Worker at `https://<worker>.<account-subdomain>.workers.dev`, one D1 database, ordinary Worker secret bindings, and a once-per-minute Cron trigger. Cron is a recovery convenience; device retries can also drive progress. No KV, Queues, R2, Durable Objects, paid monitoring, separate OAuth service, or purchased domain is required.

Run TypeScript through Wrangler. Keep the provider origin stable; changing it requires updating the registered OAuth callback and firmware public origin. Serve pairing/privacy/help pages from the same Worker. Do not expose a general database REST endpoint.

Return quickly after durable admission. Use one bounded delivery step per invocation; subsequent requests or Cron resume D1 state. A completed batch is acknowledged only through the existing exact receipt. This preserves eventual delivery without an always-running server.

## 7. Cost guarantee table

This table applies to the **selected configuration**, not every product offered by a provider. All required production rows are NO/NO; account setup must preserve that configuration.

| Component | Provider / free plan | Billing required? | Card required? | Free limit | At limit | Accept / reject |
|---|---|---|---|---|---|---|
| HTTPS API/runtime | Cloudflare Workers Free | NO | NO | 100,000 requests/day; 10 ms CPU/invocation | Invocation/request failure; cloud retry | Accept, CPU proof required |
| Ledger | Cloudflare D1 Free | NO | NO | 500 MB selected DB; daily read/write budgets in section 4 | Queries/inserts fail; retain SD history | Accept |
| Hostname and certificate | Included workers.dev | NO | NO | Provider endpoint/service policies | Endpoint may be unavailable; no purchased replacement | Accept personal-use limitation |
| Encryption/OAuth secrets | Worker secret bindings | NO | NO | Binding limits; only a handful needed | Reject configuration rather than expose secrets | Accept |
| Recovery scheduler | Worker Cron on Free | NO | NO | One of five account triggers; Free CPU applies | Delayed/failed work; request retries recover | Accept |
| OAuth project/client | Google standard OAuth, personal-use configuration | NO | NO | Consent/account/verification rules | Connection blocked; local door unaffected | Accept, callback/account gate |
| Sheets API | Google standard quota, billing unlinked | NO | NO | Standard quotas in section 4 | Pause/backoff; do not buy higher quota | Accept |
| Drive metadata/provisioning API | Google standard quota, billing unlinked | NO | NO | Standard unit quotas | Pause provisioning/archive; keep pending data | Accept |
| Sheet file storage | Free Google account allocation | NO | NO | Shared free account storage; finite Sheet size | Cannot create/write as allowed by provider; stop ACK | Accept finite capacity |
| Status/monitoring | Existing Management + provider Free diagnostics | NO | NO | Best-effort diagnostic retention | Less telemetry; no operational dependency | Accept |
| Local build/deploy tooling | Existing computer + Wrangler | NO cloud billing | NO | Local resources | Retry locally | Accept |
| Paid Workers, hosted PG, domain, quota upgrade, required paid monitoring | Any paid plan | YES or potentially YES | Provider-dependent | Irrelevant | Cost possible | Reject |

The absence of billing linkage is the spending boundary. Free quotas are not unlimited storage guarantees. Do not add a card to resolve a deployment failure; stop that external step instead.

## 8. Component diagram

```text
LOCAL AUTHORITY                         HISTORY-ONLY CLOUD
Touch / browser -> local authorization
                    -> LockController
                    -> EventLog on SD
                              |
                  closed immutable segment
                              | verified HTTPS, ingestion-only proof
                              v
                  Worker Free on workers.dev
                    |                 |
               D1 Free ledger        | Google OAuth / token refresh
            immutable reservations   | encrypted token in D1
                    |                 v
                    +-----------> Sheets / Drive APIs
                    |             fixed RAW rows + readback
                    v
             durable COMMITTED receipt
                    |
                    v
          ESP exact ACK check -> retire that segment

Cron / ESP retries -> resume durable D1 delivery steps
Google / Worker / D1 failure -> NO ACK -> SD retains pending segment
There is NO cloud-to-door command path.
```

## 9. Request/data flow

1. Owner authenticates locally on the allowed LAN Management route. Local firmware initiates existing pairing and persists pending-history protection and its pending cloud state.
2. Browser follows the backend's pairing flow, submits the short-lived claim proof securely, and completes Google consent. Preserve separate poll proof, state, PKCE, nonce and local confirmation semantics.
3. Worker durably claims provisioning; Google creates the three tabs. A lost create response is recovered by the existing deterministic Drive marker, never a blind second create.
4. After confirmation, ESP sends the original closed CSV bytes with unchanged protocol metadata and ingestion header.
5. Worker authenticates generation, validates body/CRC/hash/events, and atomically admits the batch and immutable row assignments. Admission itself is not Google delivery.
6. Respond with bounded PENDING (202) unless a matching committed receipt already exists. A best-effort `waitUntil` may advance one durable step; correctness does not depend on it surviving.
7. Cron or a later authenticated request resumes work. Read target cells, write missing exact ranges, read back all expected cells, then transactionally commit receipt eligibility.
8. ESP retries the same segment and obtains the exact receipt. Existing firmware revalidates it and retires only that segment.

Pairing, delivery, status refresh and archive creation each need persistent progress states. No callback may assume that an isolate or an in-memory Promise queue persists.

## 10. Database design

Keep the logical ledger; use SQLite-compatible D1 tables and compact internal numeric foreign keys where useful. Public IDs and wire format remain unchanged.

| Logical table | Required data/invariants |
|---|---|
| installations / connections | Installation ID, highest generation, active binding, ingestion proof hash, Google subject/email, encrypted refresh token, timezone, revision; at most one active generation |
| pairings | Existing separate proofs/hashes, encrypted PKCE/token fields, expiry, state, binding confirmation; unique open pairing |
| archives | Installation/generation/index, immutable destination and marker, reserved count, provisioning state; unique destination/marker/index |
| writer_state | Destination/tab next row; monotonically advancing counter, never derived from last visible Sheet row |
| batches / receipts | Installation/generation/segment key, original SHA, ordered IDs, receipt state, first receipt time; pending raw CSV and progress |
| deliveries | Unique installation/generation/event ID; canonical SHA, immutable destination/tab/row, frozen cells/projection version until committed |
| batch_events | Ordered membership of events in a segment, retained compactly or as equivalent validated receipt data |
| work_state | Due time, attempt count, safe error code, stage/range cursor, lease owner/epoch/expiry |
| provisioning_attempts | Durable once-only create marker and outcome uncertainty; cannot expire into permission for duplicate creation |
| migration/control state | Schema version, retirement/generation fences, capacity counters, bounded cleanup progress |

Indexes must cover authentication lookup, unique event/segment lookup, destination-row uniqueness, and due work. Do not repeatedly scan all historical deliveries to count archives or find one pending batch. Keep the existing one-pending-batch-per-binding policy. Pending payloads are bounded; unauthenticated requests cannot allocate durable rows.

Use INTEGER booleans with CHECK constraints, bounded TEXT/JSON or BLOB, explicit UTC timestamps, foreign keys, and unique constraints. Validate JSON shape and UTF-8 byte bounds. A character-length check is not an 8,192-byte CSV limit. BLOB-packed IDs/hashes are internal storage only; exact external representations remain recoverable.

## 11. PostgreSQL -> free database migration

| Current file/behavior | Reuse or minimum adaptation |
|---|---|
| `protocol.ts` parser, CRC, hashes, exact ACK keys | Preserve validation and test vectors. Buffer/crypto compatibility must be proven under Workers; runtime wrappers may change |
| `projection.ts` and event schema | Reuse mapping, 24 columns, Thai labels, RAW values, snapshot/time semantics unchanged; test timezone/Intl output |
| `types.ts`, pure protocol tests, fake Sheets scenarios | Reuse contracts and assertions |
| `database.ts` PG/PGlite and callback transactions | Replace production adapter; do not emulate atomicity with several unrelated awaited statements |
| `ledger.ts`, migrations | Port SQL and implement explicit atomic admission/commit operations; preserve constraints and public contract |
| `service.ts` Promise serialization and full work loop | Replace process-local correctness assumptions with durable stages and D1 concurrency guards |
| `pairing.ts` / `pairing-http.ts` | Preserve authorization/proof semantics; port SQL and Node request/response handling |
| `google.ts` OAuth/library/HTTP | Preserve state/PKCE/nonce/issuer/audience/scope verification and Google REST behavior. Prove library compatibility/CPU; replace only runtime transport/crypto if necessary |
| `vault.ts` | Preserve AES-GCM and binding AAD; Worker secret supplies key. WebCrypto adapter may replace Node crypto without weakening validation |
| `archives.ts` | Preserve markers, 50k policy and fixed destinations; replace advisory locks and historical COUNT with atomic counter/state operations |
| `production.ts`, `http.ts` | Replace HTTPS listener, timers and environment loader with Worker fetch/scheduled entrypoints and bindings |
| Native PG lock tests | Keep as historical contract evidence; add real D1 concurrency tests. PG passing is not D1 evidence |

Replace `$n` queries/PG casts/JSONB/`FOR UPDATE`/advisory functions with bound SQLite queries. Use versioned D1 migrations, applied as a deployment operation before compatible code—not on every request. Do not bundle `pg` or PGlite into the production Worker.

Use `nodejs_compat` only for dependencies actually exercised successfully; it is not proof that the existing Node server or Google library works unchanged. Prefer narrow fetch/WebCrypto replacements to a broad rewrite if runtime tests expose incompatibility.

There is no reported production cloud ledger to migrate yet. Start a new D1 database for staging. If an actual populated ledger is discovered later, stop writes, export and import all reservations/receipts/bindings without changing IDs or destinations, reconcile against Sheets, and only then switch origin. Never recreate counters against an existing populated spreadsheet.

## 12. Concurrency/locking design

D1 `batch()` executes a sequence transactionally and rolls back the sequence on a statement failure. It is not a JavaScript transaction callback that can hold a database lock across Google network I/O. Use primary database operations initially; do not introduce replicated reads for ACK decisions. [D1 database API](https://developers.cloudflare.com/d1/worker-api/d1-database/).

### Atomic admission

Read current revision and candidate state, compute a bounded proposal, then submit one atomic SQL batch that:

1. Asserts expected installation revision, active generation, archive identity and allowed pending state **inside the transaction**.
2. Checks existing event hashes and segment identity; conflicting data aborts.
3. Reserves rows only for new events, records frozen cell projections and ordered membership, and advances counters/revision atomically.
4. Commits the complete reservation or nothing.

An explicit SQL guard row with a CHECK constraint can force transaction failure when the expected revision is stale; delete that temporary guard within the same successful transaction. A CAS returning zero rows must not be noticed only after other statements already committed. Retry a stale proposal from a fresh primary read with a bounded attempt count.

Use set-based operations/JSON input to remain below D1's statement and parameter limits; do not generate several SQL calls for each of 32 events. Target no more than 25 statements per admission and measure actual scanned/written rows. SQL guard mechanics must be tested in D1, including rollback at every statement boundary.

### Delivery across isolates

Acquire a short durable lease using conditional SQL, random owner ID, incrementing epoch and expiry. A worker whose lease is no longer current cannot advance SQL state. Stages run below their lease duration; deadlines prevent indefinite work. On timeout, a later invocation resumes from persistent state.

**A lease is not an external Google fencing token.** A paused old worker can still finish an in-flight Google request after lease expiry. Safety therefore depends on immutable reservations: both workers can only write the same frozen cells to the same permanently reserved rows. Never reuse those rows, change receipt timestamps/projection midway, or redirect an existing reservation to a new archive. Overlapping identical writes are harmless to deduplication; a stale worker cannot commit a different binding or payload.

Check all reserved rows before the first write. Persist the inspection/write/readback stage and range cursor if the whole operation exceeds one invocation. Readback must cover all required cells before the batch becomes COMMITTED. Preserve previously verified events when retrying, but never infer an unverified range succeeded.

Provisioning claims do not use expiring leases as permission to create another file. Record the create attempt first; ambiguous completion permits marker lookup only. Archive counter/index activation uses the same revision-guard transaction as admission, and existing batches retain their original destination.

Manual edits racing a read/write are not atomically prevented by Sheets. Warning-only range protection is advisory; owners must not structurally edit managed rows. Detect observed conflicts and stop. Do not claim SQL leases solve concurrent human edits.

## 13. ACK/idempotency preservation

The COMMITTED response remains exactly:

```json
{
  "protocol": 1,
  "state": "COMMITTED",
  "installation_id": "<unchanged installation ID>",
  "link_generation": 1,
  "segment_id": "<original segment basename>",
  "segment_sha256": "<SHA-256 of original CSV bytes>",
  "event_ids": ["<complete original ordered IDs>"]
}
```

Place no additional fields into this exact receipt. PENDING/status/error responses are separate contracts. The numerical generation above is illustrative, not a reset instruction.

- Same event within the existing installation/generation dedup namespace resolves to the same reserved Sheet row, including across different segments.
- Same event ID with different canonical data is a conflict, never an overwrite.
- Lost Google write response causes readback/rewrite of those exact cells, not append.
- Lost ESP receipt causes the same stored receipt to be returned after authentication.
- Partial Google persistence can advance internal progress, but cannot produce a complete batch receipt.
- Commit receipt state transactionally only after complete validated Google readback. Database failure after Google success leaves a recoverable pending batch.
- Wrong generation, corrupt response, HTML quota page, timeout, HTTP 202 or plain HTTP 200 text never authorizes SD retirement.

The guarantee is idempotent delivery under the current link-generation model, not global deduplication after deliberate unlink/new-account history re-import. Do not create new generations automatically to evade storage limits. Preserve the existing connection-change policy and disclose intentional re-import separately.

## 14. OAuth design

Keep Authorization Code + PKCE + state + nonce. Use an exact registered HTTPS callback at `<worker-origin>/oauth/callback`; do not use wildcard/preview URLs or redirect through the ESP's `.local` hostname. OAuth callback codes are transient provider artifacts; never log query strings or token exchange bodies. Device/browser credentials remain excluded from URLs.

Keep `openid email https://www.googleapis.com/auth/drive.file`. Validate token signature, issuer, audience, expiry, nonce, verified email, subject and required scope. Preserve secure HttpOnly SameSite cookies and short pairing expiry. Google login identifies the cloud account; it does not prove local Owner authority.

Store client secret and vault key in Worker secret bindings. Encrypt refresh tokens in D1 with per-record nonce and binding-specific AAD. Cache access tokens only as an optimization; loss of an isolate must not lose the refresh capability. Keep secrets out of public variables, Wrangler files, source control, browser storage, logs and ESP firmware.

Use External Testing only during staging, with the owner added as a test user. Before unattended acceptance, use the appropriate In Production configuration and confirm refresh continues beyond the Testing expiry window. Production refresh tokens can still be revoked, expire or be invalidated; handle `invalid_grant` as reconnect-required without repeated aggressive retries.

For this personal installation, use the documented personal-use verification exception where applicable: fewer than 100 users, with a possible unverified-app warning. Do not claim ownership of `workers.dev` itself in Search Console. A provider subdomain being HTTPS does not prove Google's branding/domain verification will accept it. Register and test the exact tenant callback; if the actual console requires unverifiable domain ownership, **stop that deployment gate**—do not buy a domain or fake verification. A future public consumer service needs a separate verification/domain review. [OAuth web flow](https://developers.google.com/identity/protocols/oauth2/web-server), [verification requirements](https://support.google.com/cloud/answer/13464321?hl=en).

## 15. Google API requirements

One ordinary Google account and a Google Cloud project are needed. Enable Sheets API and Drive API; the current provisioning code uses Drive file creation and appProperties marker lookup. Sheets-only enablement is insufficient. No service account, paid Workspace subscription, Google Cloud compute, Secret Manager or Cloud Scheduler is required.

Keep billing unlinked and decline the 2026 paid quota-increase path. The setup gate must record that both required APIs work with that exact no-billing project; if an account/org policy blocks this, report the external blocker rather than activate billing.

Use a conservative application budget of at most ten Sheet reads and ten writes per minute for this installation, plus a small provisioning burst within published limits. Count retry traffic and throttle per account/project, not only per ESP. Persist due times so Cron and device retries do not multiply the rate. Group contiguous ranges and request only needed fields.

Treat 403 by safe Google error reason, not status alone: quota, permission, invalid grant and storage issues require different recovery. Never persist raw provider error bodies. A moved file retains its ID; lost permissions or deletion block the destination. Do not silently create a replacement file with reset row counters.

## 16. TLS design

Cloudflare terminates public HTTPS on the included hostname. ESP continues chain/hostname/time verification using its existing public trust-anchor mechanism. At deployment inspect the actual chain and choose the appropriate long-lived root; do not assume all `workers.dev` certificates always use one CA. If needed, include two justified roots for planned rotation and remeasure size. No leaf pinning, insecure TLS or paid certificate.

Worker outbound fetch uses platform HTTPS trust to fixed Google hosts. Reject arbitrary user-provided provider URLs and redirects carrying authorization. OAuth browser redirects are a separate intentional browser flow.

**HTTP framing is an acceptance gate:** current firmware rejects chunked replies and redirects. Return a bounded JSON string/TypedArray or FixedLengthStream, allowing the Worker runtime to calculate Content-Length; setting that header manually is insufficient. Verify actual edge HTTP/1.1 output, including compression behavior, with the ESP parser. Keep responses uncompressed and `no-store`; do not change firmware to accept unbounded transport. [Worker Response framing](https://developers.cloudflare.com/workers/runtime-apis/response/).

## 17. Quota exhaustion behavior

| Failure | Backend/sender behavior | Local result |
|---|---|---|
| Worker daily requests or CPU exhausted | Invocation fails; response may be provider HTML or absent | Reject as ACK, retain segment, backoff |
| D1 daily read/write quota | Fail admission/work/commit; no success fabricated | Retain data until reset and retry |
| D1 storage full/high-water | Stop new admission; allow safe recovery/compaction where possible | Pending SD protected; no automatic paid DB |
| Google 429 or quota-related 403 | Persist due time if possible; return safe transient status | Exact batch stays pending |
| Google storage full | Pause writes/provisioning; owner can manage existing storage separately | No auto deletion or paid storage |
| OAuth revoked/expired | Mark reconnect-required; stop pointless delivery attempts | Local Owner/access unaffected |
| Backend/Google DNS/TLS/outage | Bounded timeout, no receipt | Local access/touch/relock independent |
| SD protected capacity exhausted | Preserve older pending rows; existing refusal/loss-counter policy | New log history can be lost; door still works |

Retain existing ESP 15/30/60/120/240/480/900-second backoff with bounded jitter and safe Retry-After handling. Backend delays must persist, including when a new request arrives. Cron should inspect a bounded due-work index, not hammer Google every minute during a known quota pause. If D1 itself is unavailable, rely on sender backoff until storage returns.

Thai status: generic unavailable/uncertain errors use **“การซิงก์ถูกพักชั่วคราว”**. Only a trustworthy backend classification may show **“โควตาบริการฟรีเต็ม กรุณารอรอบโควตาถัดไป”**. An HTML Cloudflare error cannot safely be classified as a particular quota. Storage-full is not a daily reset condition; show **“พื้นที่เก็บประวัติเต็ม การซิงก์ถูกพักชั่วคราว”**. Avoid promising automatic recovery for permanent capacity failures.

## 18. Backend data retention

Do not keep raw CSV, canonical JSON, frozen 24-cell projections and duplicate receipt material forever. Conversely, do not TTL-delete event dedup records while their generation can still authenticate.

Recommended two-stage policy:

1. **Pending/blocked:** retain original raw batch, frozen cells, canonical validation data, exact row assignment and progress. Never garbage-collect an uncertain delivery for age alone.
2. **Committed:** atomically preserve a compact receipt and event tombstones, then release redundant raw/projection payload. Tombstone stores internal binding reference, event ID, canonical SHA-256, destination/tab/row and committed state. Receipt stores segment identity/hash and complete ordered event membership. On replay, validate uploaded data against those hashes before returning receipt. This relies on the same cryptographic SHA-256 collision-resistance assumption already used by the protocol.

Permanent active-generation tombstones are necessary for cross-segment dedup. Storing only the latest segment or deleting receipts after 30 days is unsafe. Expired pairing proofs and unnecessary token material can be removed in bounded cleanup; revocation/generation fences must remain. A retired generation's bulky data may be removed only if it can never authenticate again and recovery will not recreate its reservations against the old Sheet.

Planning estimate, **not a measured D1 result**: compact records plus indexes/membership should target roughly 0.5–1 KB/event, depending heavily on segment fill. Reserve 100 MB for pending data, metadata, indexes and cleanup; a 400 MB history budget then holds about 400,000–800,000 events. At 100 events/day that is roughly 11–22 years; at 1,000/day roughly 1.1–2.2 years. Measure populated D1 fixtures before acceptance; do not present these numbers as guaranteed capacity.

Warn before approximately 350 MB and pause new admission by 400 MB unless measured overhead requires an earlier threshold. Keep headroom for completing admitted batches and administrative cleanup. No automatic sharding, account farming, generation rollover or paid upgrade. More Free D1 databases are a potential future redesign, not unlimited capacity or part of this implementation.

**Unbounded exact dedup plus unlimited history cannot fit forever in finite free storage.** This design chooses the user's explicitly allowed safe sync pause at exhaustion. It never sacrifices idempotency to create the appearance of unlimited free operation.

Time Travel is limited recovery, not proof of exactly-once state after a database rollback. Restoring old reservations can collide with newer Google rows. Freeze delivery after restore and reconcile ledger against Sheets before accepting uploads; never automatically resume a rolled-back ledger with stale row counters. Losing the ledger or vault key requires deliberate recovery, not fresh counters against existing history.

## 19. Google Sheet/archive retention

Preserve the existing three Thai tabs: **ประวัติการเข้าใช้งาน**, **เหตุการณ์ระบบ**, **สถานะระบบ**. Preserve all 24 existing event columns and human/technical column separation. This redesign introduces no new event format or competing cloud log.

Continue approximately 50,000 reserved events per archive generation. At 24 event columns, 50,000 events consume approximately 1.2 million event cells, plus headers/status and allocated blank grid. Keep allocated grid bounded across both event tabs; even allocating 50,000 rows to each is around 2.4 million cells, below the current 10-million-cell file limit. [Google Sheet size limit](https://support.google.com/drive/answer/37603?hl=en-GB).

Archive creation must preserve deterministic markers and old batch destinations. Old spreadsheets stay in the owner's account; do not delete or compact them automatically. Offer clear archive links through existing status metadata when applicable, without scanning all files on each upload.

Free Google account storage is shared with Gmail/Photos/Drive, normally 15 GB. Full storage can block new files; archives do not create unlimited free space. Never purchase Google One automatically or delete unrelated account data. [Google storage limits](https://support.google.com/drive/answer/6374270?hl=en).

## 20. ESP32 impact

Target firmware changes are public backend origin and public CA anchor only. Keep CSV schema, installation/generation handling, exact ACK, protected retention, TLS worker, request limits and local authorization unchanged. A small Thai quota/status mapping is optional if existing generic status cannot represent the required state; it must not broaden ACK parsing.

No Cloudflare SDK, OAuth code, Google refresh token, database client or new cloud authority belongs on ESP32. Keep the current 8 KB/32-event upload ceiling and response bounds. Backend processing may split work internally but must not require the ESP to delete a partially acknowledged segment.

Rebuild the **enabled-cloud** configuration to track growth from 1,271,792 bytes and 38,928-byte headroom. No new size or RAM claim is made in this design. Measure heap/minimum heap/largest block during active repeated TLS, not only cloud-disabled idle operation. Do not change the partition table.

## 21. Security analysis

The backend has history ingestion, pairing, OAuth, status and disconnect functions only. No unlock/lock endpoint, identity/PIN/Wi-Fi mutation, local Owner decision, remote shell or firmware update. A backend compromise could damage history/privacy but must not become a valid local door credential.

Keep body/header limits before parsing, authenticating before durable allocation. Bound pairing attempts and unauthenticated requests without creating one D1 record per attacker request. A public endpoint can be abused into free-quota exhaustion; free hosting does not provide guaranteed anti-DoS availability. Accept cloud pause, not payment escalation.

Secret exclusions remain browser credentials/verifiers, PIN/verifier, Wi-Fi/AP passwords, enrollment and Management tokens, ingestion keys, OAuth tokens and cryptographic secrets. Upload only existing validated event fields. Use safe error codes and no request-body/query logging. Names and access times are private personal history; keep Sheets private by default and write formula-looking values with RAW semantics.

Use least-privilege deployment credentials, MFA for account owners, encrypted token vault with externally retained recovery key, and separate staging/production D1 bindings. Host public privacy/help text on the same free origin if needed. CORS is not authentication. Protect browser state-changing endpoints with existing proof/session/CSRF semantics and exact allowed origins.

## 22. Factory Reset/disconnect behavior

Preserve current disconnect semantics: disable ingestion binding and delete its backend refresh token; leave Sheets intact and pending SD history protected. Google account-level application access can be revoked separately in Google account settings. Do not revoke unrelated account grants blindly.

Preserve existing physically authorized Factory Reset behavior; do not perform it in this migration. Local reset clears local integration/installation state under current reset policy. It does not delete remote Sheets. A reset while offline cannot guarantee immediate backend token deletion; remote disconnection/account revocation remains available to the owner. Old installation/generation state must never become a new local authorization path.

Cloud outages must not turn reset or disconnect into a prerequisite for ordinary access. No migration should reset Owner, identities, PIN, Wi-Fi, browser credentials or existing logs.

## 23. USER ACTIONS

### One-time account setup

1. Sign in/create a Cloudflare Free account and authorize local Wrangler access. Account login, MFA, terms and granting deployment access are account-owner actions. No domain purchase, payment method or paid Workers plan.
2. Sign in to the Google account that will own history, and authorize access to a Google project with billing unlinked. Codex can create/configure the project and APIs where authenticated tools permit; the owner handles account verification, consent and any console-only approval.
3. Approve the OAuth application's identity/contact details and permitted scopes. Codex can prepare the exact callback and settings, securely install the client secret, and automate supported configuration. Staging may require the owner as test user; final unattended use requires the correct production/personal-use setting.

Do not ask the user to manually write SQL, create columns, paste spreadsheet IDs, copy script URLs, enter tokens in SmartLock, or perform technical deployment commands that authenticated tooling can execute. If the console imposes a mandatory card/billing/owned-domain condition, do not bypass it; report that specific gate.

### Final Google consent

Owner opens Management → ระบบ → Google Sheets → **เชื่อมต่อ Google**, selects the intended account, grants the requested file permission, and confirms the binding through the existing flow. This actual account consent cannot be fabricated by Codex. Sheets and columns are then created automatically. Consent may need repeating only after revocation, account change or genuine authorization expiry.

## 24. CODEX ACTIONS

After a later implementation authorization, SOL can direct/review bounded LUNA work to:

- Add Worker entrypoints/configuration and D1 migrations; port only infrastructure-dependent code.
- Preserve protocol/security test vectors and implement D1 transaction/concurrency/crash tests.
- Configure the stable public origin, OAuth callback, non-secret metadata and three-tab provisioning.
- Deploy using authenticated account tools, only after verifying Free/no-billing configuration.
- Generate/install Worker secrets securely through authenticated CLI; never print secret values or request them in ordinary chat.
- Prepare Google API/OAuth settings and automate supported steps; leave login, consent, MFA and required account-owner decisions to the user.
- Validate fixed-length HTTPS responses and CA trust, then change firmware public origin/CA only as needed.
- Build, review flash/RAM, and—only under later explicit deployment authorization—flash COM6 normally without reset or credential changes.
- Run mock/D1/real Google/staging/board tests and active-cloud soak; compare non-secret state before and after.
- Verify exact rows, receipts and no-billing status, then produce a traceable acceptance report.

None of these implementation/deployment actions were performed by this design task.

## 25. Migration phases

**Phase F1 — local feasibility and D1 contract.** Add a separate Worker target and SQLite migrations, preserve the conventional implementation as reference, and prove atomic reservations/rollback/receipt behavior in local workerd/D1 tooling. Exercise max-size parsing, OAuth crypto and all range patterns for CPU/query budgeting. No board or Google account mutation.

**Phase F2 — authenticated Free staging gate.** After account-owner setup and explicit deployment authorization, create one staging Worker/D1 on Free. Prove real CPU margins, HTTP/1.1 framing, Cron recovery, no-card/no-billing settings, exact Google callback and real account consent. Stop if a hard cost condition cannot be met; no paid fallback.

**Phase F3 — delivery migration and fault acceptance.** Complete resumable writer, compact receipts, bounded cleanup, archive CAS, quota classification and reconnect behavior. Run real Google staging crash/lost-response/duplicate tests. Do not migrate a real populated ledger by resetting counters.

**Phase F4 — enabled firmware and production acceptance.** Following SOL review, configure real origin/CA, build, perform authorized normal flash/reboot and active-cloud soak. Preserve all local state. Capture physical-only checks separately and report remaining limitations honestly.

This is a moderate backend migration with substantial concurrency verification, not a URL-only deployment task. Firmware scope should remain small.

## 26. Test strategy

| Layer | Required evidence |
|---|---|
| HOST TEST | Existing protocol/schema/CRC/Thai snapshot/RAW/ACK fixtures; wrong generation, conflicting ID, wrong proof, OAuth state/nonce/scope/token expiry; compacted receipts match pre-compaction behavior |
| LOCAL D1/WORKER TEST | Atomic rollback at every statement, stale CAS, concurrent same/different batches, archive boundary, disconnect races, lease expiry with old worker resuming, immutable projection, ordered receipt preservation |
| MOCK RECEIVER / GOOGLE TEST | Lost upload reply, lost Google response, partial writes, malformed/oversize response, 401/403 reason/429/5xx, deleted file, delayed provisioning lookup, expired token, quota and backend outage |
| REAL FREE WORKER/D1 TEST | Verify actual plan/account gates; max 8 KB/32-event CPU with cold/warm execution; D1 statement/parameter/row quotas; multi-isolate concurrency; fixed-length HTTP/1.1; Cron recovery; no durable reliance on waitUntil |
| REAL GOOGLE TEST | Owner consent in staging; exact three tabs; one event one reserved row after duplicate uploads; full readback before receipt; archive at reduced fixture threshold; no duplicate Sheet on uncertain creation; refresh beyond Testing window |
| LIVE ESP32 TEST | Verified TLS/invalid CA/date failure; no redirect/chunked acceptance; exact segment retirement; interrupted transfer/reboot; quota retention; unchanged auth/Owner/PIN/Wi-Fi/calibration; healthy timed relock scheduler |
| SOAK | Active cloud plus direct-LAN probes, repeated TLS success/failure, DNS outage, backend unavailable, synthetic quota responses, heap recovery and largest block, touch/HTTP responsiveness, no watchdog |
| PHYSICAL USER TEST | Optional final real Access/magnet observation only when safely supervised; software logs cannot prove magnet movement |

Do not exhaust real service quotas or fill production SD to test limits. Inject failures in staging and use copied fixtures. Test protected-full behavior and loss counters on host/storage fixtures. Preserve the reported intermittent LAN/TCP observation as an acceptance question; do not redesign Wi-Fi from speculation.

Require measured Free CPU headroom, not merely an average below 10 ms. Worst-case fragmented Google ranges, cold key verification, Thai UTF-8 and all legacy schemas must be covered. If a single necessary step exceeds budget, split bounded stages or narrow new runtime dependencies; do not silently upgrade Workers.

Restore tests must cover a D1 snapshot older than Sheet writes: synchronization must freeze until reconciliation, not reuse stale row allocations. Test missing vault key, malformed compact receipt, corruption and schema rollback incompatibility as explicit blocked states.

## 27. Risks and tradeoffs

1. **Free CPU is tight.** I/O waiting is cheap in CPU terms, but CRC, JSON, library initialization and signature verification still cost CPU. Production feasibility requires real measurements; no paid escape hatch.
2. **OAuth tenant-domain acceptance remains an external gate.** Personal-use exemption and HTTPS callback syntax support this proposal, but no account was configured here. Public verified distribution cannot be promised on a shared provider domain.
3. **Finite storage cannot retain infinite exact dedup/history.** Compact D1 records and 50k Sheet archives postpone capacity limits; they do not remove them. Safe pause is the permitted outcome.
4. **D1 is not PostgreSQL.** Incorrect CAS/transaction porting can allocate duplicate rows or acknowledge incomplete history. Reuse guarantees and tests, not database syntax blindly.
5. **Google paid-tier changes are already announced for 2026.** Keep billing unlinked, never opt in, and recheck official terms at deployment. No provider's free plan is a contractual perpetual guarantee.
6. **Free cloud availability is best effort.** Public quota abuse, provider outages, account suspension and workers.dev policy changes can stop history sync. Door authority remains local.
7. **Ledger rollback/loss and manual Sheet editing need deliberate recovery.** Fixed rows protect retry safety, not arbitrary owner edits or stale database restores.
8. **Active-cloud LAN reliability remains unproven.** Prior cloud-disabled soak had transient failures; later short observation recovered. Acceptance must test the enabled configuration.

Decision remains **A** for the existing personal installation under these explicit gates. This is not decision C merely because free quotas exist—the user expressly allows sync pauses. If account setup requires billing/card or the mandatory runtime cannot fit Free after bounded adaptation, A fails its gate and no implementation may silently relax the requirement.

## 28. Exact next instruction for SOL

> Implement Phase F1 only of `docs/design/GOOGLE_SHEETS_FREE_ONLY_ARCHITECTURE.md`: add a local Cloudflare Worker/D1 target while preserving the current ESP protocol and production firmware. Port atomic admission, immutable row reservations, exact receipts and resumable delivery using D1-safe transactions/CAS. Preserve OAuth/pairing/vault security and fixed-row Google semantics. Add deterministic concurrency, stale-lease, rollback, lost-ACK, compaction and quota tests against the Worker/D1 runtime. Measure maximum-size CPU/query budgets and report what still needs real Free staging proof. Do not deploy, access COM6, create OAuth credentials/Sheets, enable billing, add a card, change Owner/PIN/Wi-Fi/browser storage/logs, or reset the device in this phase. SOL reviews all security/storage/ACK changes. Stop only the affected external step if a Free/no-billing condition cannot be satisfied. Do not rewrite working local architecture or change the partition table.

After F1 evidence passes, request only the account-owner authentication/consent genuinely needed for F2. Technical account setup and deployment should otherwise be automated under the user's later authorization.
