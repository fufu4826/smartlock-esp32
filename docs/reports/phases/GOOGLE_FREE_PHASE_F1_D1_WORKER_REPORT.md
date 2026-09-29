HISTORICAL: Google Sheets is removed from the active product. This document records an earlier design or test checkpoint and is not current implementation authority.

# Google Free Phase F1 — Worker/D1 local feasibility

> **HISTORICAL / SUPERSEDED (2026-09-27):** This report records the earlier Workers/D1 feasibility phase and remains available as evidence. The approved current architecture is [Google Sheets one-click](../design/GOOGLE_SHEETS_ONE_CLICK_ARCHITECTURE.md). F1 evidence is local-only and does not establish current deployment or Google acceptance.

Date: 2026-09-27. Baseline: `0ce8c663769c456325d19b2b3439297d8ebb9ca6`.
Authority: `docs/design/GOOGLE_SHEETS_FREE_ONLY_ARCHITECTURE.md` and the user's F1-only request.

## 1. Overall result and review boundary

**F1 PASS — local feasibility only.** This is not production-cloud acceptance or permission to start F2. SOL reviewed the schema, every new source module, admission/ACK/compaction, pairing/vault, durable work, HTTP boundary, measured budgets and dependency configuration. Luna High provided bounded runtime/test work; SOL integrated, corrected, tested and approved it. Workers/D1/Google external gates remain below.

No production firmware, existing backend source, partition table, ESP protocol, COM6, Owner, identity, PIN, Wi-Fi, browser storage, or physical/local event history was changed. No Google account, OAuth credential, real Sheet, Cloudflare login, remote database, deployment, billing or payment setup was accessed. The original PostgreSQL backend remains intact.

Unrelated existing changes to `FINAL_FROM_ZERO_REPORT.md`, `STALE_ENROLLMENT_REPORT.md`, and `evidence/final_from_zero/after-phone2.txt` are excluded from this commit.

## 2. Worker project structure

`backend-worker/` contains a separate TypeScript entrypoint, D1 ledger, delivery state machine, pairing coordinator, crypto/metadata/runtime helpers, migration, local-only Wrangler configuration, free-configuration check and focused tests. Wrangler 4.116.0 / Miniflare 4.20260730.0 / workerd compatibility 2026-07-30 are pinned with a lockfile. No application runtime package was added; the build shares existing source modules.

Commands: `npm ci`, `npm run typecheck`, `npm run check:free`, `npm test`, `npm run build`. `npm run dev` is local only. No deploy script exists. The bundled Worker is approximately 66.4 KiB before compression; this is not ESP flash growth. Firmware was neither edited nor built.

## 3. Reused implementation

The Worker imports the existing `backend/src/protocol.ts`, `projection.ts` and `types.ts`: exact CSV formats, CRC32, SHA-256, canonical events, identity snapshots, Thai Sheet tabs/cells and seven-field ACK type. Tests reuse recorded/synthetic fixtures, the original fixed-range fake Sheets adapter, and the Node vault as a compatibility oracle. No second event wire format was invented.

Existing legacy/current/schema-3 fixtures execute inside actual workerd. The worst-practical fixture is exactly 8,192 bytes, 32 schema-3 NTP events, Thai UTF-8 names, escaped CSV, CRC, SHA and all 24 projection cells.

## 4. PostgreSQL-specific replacements

The new target replaces PostgreSQL transactions/advisory locks with a single D1 transactional batch, CHECK-backed abort guards, revision/generation CAS, unique constraints and short durable leases. SQL reservations are set-based using `json_each` and window functions. No Node HTTPS server, continuous daemon, PostgreSQL client, advisory lock or long-running promise is required by the Worker.

## 5. D1 schema

`migrations/0001_ledger.sql` defines control, installations, connections, archives, writer_state, batches, deliveries, batch_events, work_state, provisioning_attempts, pairings, oauth_revocations and transaction-local guard records. Foreign keys bind membership to batches/events and rows to destinations. Partial unique indexes enforce one active binding/archive/open pairing and one pending-or-blocked batch per generation.

Unique `(installation,generation,event)` and `(destination,tab,row)` constraints prevent duplicate reservations. Triggers freeze content hashes, projection/destination/row and segment receipt metadata. Projection/raw bytes can be nulled only by the reviewed post-COMMITTED compaction path. Due-work indexes exclude terminal records; authorization, event lookup, membership and receipts use indexed keys rather than history scans.

## 6. Atomic admission

Preflight authenticates and validates immutable bytes. Inside one nine-statement D1 batch, a CHECK guard requires the expected installation revision, active generation/binding, exact current archive/destination, capacity and unfrozen control state. The transaction inserts batch/deliveries/membership/work, advances tab counters and archive count, increments revision and removes its guard. Any statement failure rolls back everything.

Same-event different-content conflicts before admission and remains protected by CAS/uniqueness against races. Same-batch retry returns its existing reservation or receipt. Only one immediate CAS retry is allowed; more contention fails without ACK and is left for the sender's later retry. Two attempts remain below the Free per-invocation query ceiling. No partial receipt is returned.

## 7. Concurrency strategy

Tests create genuinely overlapping promises against real local D1, including barrier-controlled preflight races and overlapping actual workerd HTTP requests. Same batches converge; different batches cannot both become pending; counter allocations remain unique. Archive capacity and activation races are guarded inside the transaction. Stale revision/generation and disconnect races cannot admit an old binding.

A lease is explicitly **not Google fencing**. Both old and new invocations can only issue the same frozen values to the same immutable rows. D1 progression/commit additionally checks lease owner/epoch/expiry, stage/cursor/progress hash, generation, active binding and control state. Tests hold a fake write while another invocation takes over, then release the stale write. Disconnect/account-generation change during an outstanding write prevents stale commit. An already issued history write may finish after disconnect; it cannot authorize the door or produce a valid old-generation ACK.

## 8. Durable work state

Stages are INSPECTING → WRITING → VERIFYING → COMMITTING → COMMITTED, with BLOCKED for integrity/destination conflicts. Stage, range cursor, missing IDs, progress digest, attempts, due time and lease are persisted. Each invocation advances one range or one stage. The default lease is 30 seconds and each adapter operation has a 10-second deadline; a writing step performs at most a read plus a write.

Every writing range is re-inspected immediately before writing, then all reserved cells are read back during verification. This catches a conflicting user edit between earlier persisted inspection and writing. Google offers no atomic compare-and-swap with its users: an edit racing the final read/write remains an operational limitation of fixed-row Sheets, requiring the existing protected/service-owned tab policy. No claim of impossible cross-system fencing is made.

Quota/lost-response/partial-write errors return to inspection with 15-second exponential backoff capped at 15 minutes. Deleted destinations, mismatching cells and corrupt pending/work data block without ACK. Restart tests close/reopen D1 and restore a durable fake Sheets file after admission and after a persisted write. The actual workerd scheduled handler also completes delivery from D1. Isolate memory and `waitUntil` are not correctness dependencies.

## 9. Exact ACK preservation

Only these fields are emitted: `protocol`, `state`, `installation_id`, `link_generation`, `segment_id`, `segment_sha256`, `event_ids`.

COMMITTED requires active current binding, unfrozen control, committed work/batch, matching segment hash and complete ordered committed membership. SQL commit occurs only after exact cell verification. Lost backend/ACK responses recover the same receipt. HTTP 200 from a fake/real future adapter is never cleanup authority. The existing ESP wire protocol is unchanged.

## 10. Completed-record compaction

Compaction requires COMMITTED batch and members. It releases raw CSV, canonical text and cells while preserving canonical/cell hashes, event binding, destination/tab/row, projection version, committed state, segment hash and ordered receipt membership. Pending and blocked recovery information is retained. No TTL deletes active-generation dedup state.

Tests cover exact replay after compaction, D1 close/reopen, and the same compacted events in a later segment without allocating any additional row. Historical remote edits after a previously verified commit do not invalidate the durable original receipt or cause re-upload; this preserves exact-once row allocation rather than making Sheets current contents authoritative.

## 11. OAuth/crypto runtime compatibility

WebCrypto SHA-256, random state/nonce, PKCE S256 and AES-256-GCM execute in workerd. Vault records interoperate both ways with the existing Node vault (`nonce || tag || ciphertext`, base64), with binding-specific AAD and tamper/wrong-binding rejection. Claim/poll/browser/device proofs remain separate; callback completion cannot activate a connection without device confirmation.

Metadata checks retain issuer, exact audience, live expiry, nonce hash, verified email and drive.file grant requirements. Google scope canonicalization is accepted as in the reference verifier. **Metadata checking is not JWT signature verification**; the real provider remains unconfigured. F2 must implement signed-token/JWKS verification and bounded real token/Sheets calls before exposure. Tests use synthetic provider secrets only.

Provisioning has a persistent once-only create claim; uncertain create responses retry marker discovery without a second create authorization. OAuth restart invalidates an in-flight old callback. Concurrent claim/confirm keeps one browser and binding. Disconnect is proof-authenticated, CAS-guarded and retryable; an old disconnected proof cannot disable a replacement generation.

## 12. HTTP framing and security boundary

The route allowlist contains health, event ingestion, cloud status/disconnect, pairing and future OAuth callback/start only. Tests reject door/lock/PIN/identity/Wi-Fi/Owner/debug/OTA/backup paths. There is no cloud-to-door path.

Ingestion has an 8,192-byte body bound; flat management/pairing messages have a 1,024-byte bound. Headers are bounded, MIME/security metadata validated, unexpected query strings rejected, and body reads have an absolute three-second deadline. Fixed JSON responses are bounded to 4,096 bytes with no-store/no-transform and identity encoding. Ingestion never redirects.

Actual local workerd over TCP emitted Content-Length matching bytes, no Transfer-Encoding and identity encoding. Actual Worker ingestion/scheduled/retry returned exact COMMITTED. **Real workers.dev TLS, HTTP/1.1 framing, transformations and ESP compatibility remain unproved.** F1 status deliberately reports only local binding state; real Sheet/account status remains F2 adapter work.

## 13. CPU measurements

Evidence: `evidence/google_free_f1/mechanism-budget.json` and `budget.json`. Node repeated-process CPU is a local proxy, not an isolated Worker measurement. Worst 8-KiB/32-event parsing/CRC/SHA/projection measured **6.10 ms/iteration** averaged over 100 iterations in the final budget run (610,000 microseconds total user+system CPU); the separate regression run measured 5.47 ms. OAuth crypto measured **1.10 ms/iteration** over 100 iterations. D1 admission driver CPU includes Miniflare proxy overhead and must not be mistaken for edge CPU; workerd's 40-ms elapsed interval across all parser samples likewise is not CPU time.

**The actual Free 10-ms CPU budget is not certified in F1.** Cold/warm request, maximum payload, delivery stages, ACK and signed OAuth verification need real Free runtime measurements in F2. If it fails, reduce bounded work safely or report failure; do not upgrade. Current documented Free HTTP/Cron CPU and request limits: [Cloudflare Workers limits](https://developers.cloudflare.com/workers/platform/limits/), checked 2026-09-27.

## 14. D1 statement/read/write measurements

Maximum-size 32-event local ledger measurements:

| Operation | SQL statements | Rows read | Rows written |
|---|---:|---:|---:|
| First admission including pending ACK lookup | 15 | 308 | 205 |
| Pending exact retry | 3 | 6 | 0 |
| COMMITTED ACK lookup | 2 | 69 | 0 |
| COMMITTED exact retry | 4 | 73 | 0 |

HTTP adds one proof precheck to ingestion. The atomic admission portion is nine statements regardless of event count. A complete delivery spans separate bounded invocations; total lifecycle statements must not be confused with per-invocation usage. D1 metadata includes index writes; values are local measurements, not a remote quota bill. The 1,000-event experiment also records complete lifecycle totals with/without compaction.

Current documented Free limits include 50 queries/invocation, 5 million rows read/day, 100,000 written/day and 500 MB/database. Sources: [D1 limits](https://developers.cloudflare.com/d1/platform/limits/) and [D1 pricing](https://developers.cloudflare.com/d1/platform/pricing/), checked 2026-09-27. Account quota/remote accounting must be measured again in F2.

## 15. Test result inventory

**56 distinct focused F1 tests PASS.** The final regression command ran 54/54 successfully; the separate populated-budget command ran 3/3, including one parser case common to both commands (54 + 3 - 1 = 56). No skipped or failed cases occurred in either final command. This avoids repeating the already-running storage population while still testing the final schema-3 fixture and reviewed source.

The suite covers D1 transactional/concurrency/recovery tests, actual workerd HTTP/scheduled/crypto tests, parsing/projection compatibility and populated storage measurements. Final TAP files are retained beside the JSON budget evidence. Typecheck, bundle, configuration guard and full npm audit pass. The existing backend regression is recorded separately below; it is not counted as D1 proof.

Initial test runs exposed fixture-format and Windows cleanup errors, which were corrected. Review additionally found the inspection-to-write edit window and disconnect retry/CAS gap; both received source corrections and focused regression tests. Earlier failed runs are not presented as successful acceptance evidence.

## 16. Failure/recovery evidence

| Required case | Local evidence |
|---|---|
| Concurrent same/different batch, counter race | Barrier-overlapped actual D1; same batch also overlaps HTTP |
| Same events across segments / different content | Reuse existing row / reject conflict |
| Stale revision/generation/archive boundary | In-transaction guard rejects or safe retry uses current archive |
| First/middle/final admission failure | All nine statement positions roll back batches, rows and counters |
| Admission/write interruption, ledger reopen | Close/reopen durable D1 and durable fake Sheet, resume and exact ACK |
| Expired lease and late old invocation | Frozen row/content unchanged; stale stage advancement rejected |
| Lost ACK / backend reply | Exact committed retry returns same receipt |
| Lost write response / partial write / quota | No early ACK; backoff, inspection and exact recovery succeed |
| Deleted destination / edited row | BLOCKED; no ACK |
| D1 read/write/commit failure | No admission/false ACK; commit retry recovers |
| Corrupt pending progress | BLOCKED; no ACK |
| Vault absent, expired pair, stale callback | No account binding activation |
| Provisioning uncertainty | One create claim; marker recovery reuses destination |
| Disconnect/generation change during write | No old-generation COMMITTED |
| Compacted replay and later duplicate segment | Receipt retained; no new reservation |
| Cron-equivalent recovery | Real workerd scheduled invocations advance persisted stages |

If Worker/D1/Google is unavailable or quota-limited, no valid exact COMMITTED receipt is produced. The unchanged firmware's existing protected-segment policy therefore remains in force. Local door independence is a source-boundary observation in F1, not a new physical test.

## 17. Existing backend regression

All 83 existing backend cases are covered successfully: the initial run passed 80, while three native-PostgreSQL cases failed because the dedicated local fixture at 127.0.0.1:54329 was stopped. Starting only the existing `smartlock-google-stage` container and rerunning those three cases passed 3/3 (native PostgreSQL, archive threshold, verified HTTPS API end-to-end). The fixture was stopped afterward. No production database was used or changed. This is reference-regression evidence, not a substitute for the separate D1 tests.

## 18. D1 storage growth

`budget.json` records two actual local D1 databases populated through admission, verified delivery and ACK replay with 1,000 schema-3 events including Thai identity snapshots. One retains full records; one compacts each completed batch. `meta.size_after` is allocated SQLite size, including indexes/free reusable pages; it is not live-text length and compaction does not necessarily shrink the file.

Baseline allocation was **172,032 bytes**. At 1,000 events, uncompacted allocation was **2,445,312 bytes**; compacted allocation was **2,228,224 bytes**. Incremental allocation is therefore approximately **2,273 bytes/event uncompacted**, **2,056 bytes/event compacted**. Compaction saved 217,088 allocated bytes in this fixture; it is not a claim of dense packing or reclaimed filesystem space. A conservative planning allowance is roughly 2.1 KB/event plus baseline/headroom, subject to remeasurement at larger scale.

Across 32 completed batches, the compacted experiment used 2,272 statements, 47,281 rows read and 10,208 rows written for admission, work, replay and compaction combined. These are complete-population totals across many invocations, not one request. At this measured density, a 500-MB database is finite on the order of a few hundred thousand events; reserve operational space and pause before the limit.

Use the recorded baseline-subtracted bytes/event as a planning estimate, not an unlimited-storage promise. Permanent receipts/tombstones still grow. Stop safely before the Free database ceiling; never delete dedup state to pretend capacity is unlimited and never enable a paid fallback. Remote page allocation and larger histories require F2 confirmation.

## 19. Free-only dependency audit

Configuration contains only workers.dev and one placeholder local D1 binding; no purchased domain, PostgreSQL hosting, R2, KV, Queues, Durable Objects, paid monitoring or automatic deploy/upgrade dependency. `npm run check:free` checks infrastructure shape and rejects nonlocal IDs at this F1 checkpoint. Dependency audit reports zero vulnerabilities. Test-only Miniflare internals/fake service bindings are local tooling, not production cloud infrastructure.

The static guard cannot inspect real account billing, plan selection or future provider pricing. Real Free/no-card/no-billing settings are mandatory external gates. Quota exhaustion permits synchronization to stop; it never permits a paid substitute.

## 20. Exact F2 gates and recommendation

**Proceed to separately authorized F2 staging, not production rollout. Stop here for this request.**

1. Confirm a genuine Cloudflare Free account, no card/paid subscription/automatic overage, D1 Free and workers.dev only; Google billing unlinked and no trial. Stop if any cost condition cannot be met.
2. Under new authorization only, create staging Worker/D1 and measure real cold/warm CPU, startup, query/row accounting, maximum 8-KiB ingestion, delivery stages, ACK, Cron recovery and multi-isolate races.
3. Prove workers.dev HTTPS certificate chain, bounded identity-encoded HTTP/1.1 Content-Length responses and existing ESP parser compatibility. No firmware protocol workaround is authorized by F1.
4. Complete real OAuth/JWKS signature verification and bounded Google adapters; verify the exact workers.dev redirect can be configured and consent granted without a purchased domain. Recheck provider free/no-billing terms then. No live OAuth is claimed here.
5. In a real staging Sheet, test marker recovery, protected exact rows, partial/lost responses, account revocation, destination deletion, archive activation, full readback and exact receipts. Complete connection/status reporting and durable revocation handling before real rollout.
6. Test D1 snapshot restore older than Sheet writes under a frozen/reconciliation procedure; never resume stale row counters blindly. Measure long-history storage and paused-quota behavior.

No F2 action was taken. Physical hardware testing is outside F1 and has no fabricated PASS entry.
