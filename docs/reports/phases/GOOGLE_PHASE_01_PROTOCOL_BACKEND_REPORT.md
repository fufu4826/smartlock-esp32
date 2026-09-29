HISTORICAL: Google Sheets is removed from the active product. This document records an earlier design or test checkpoint and is not current implementation authority.

# Google Sheets Phase 1 — local protocol/backend proof

> **HISTORICAL / SUPERSEDED (2026-09-27):** This report records an earlier local backend phase and remains available as evidence. The approved current architecture is [Google Sheets one-click](../design/GOOGLE_SHEETS_ONE_CLICK_ARCHITECTURE.md). Its historical PASS applies only to the scope stated below; it does not establish current deployment or Google acceptance.

Date: 2026-09-27. Source checkpoint: `9613716a0843b575a9cf3f90be8cbff3ae1c8032`.

**Overall result: PHASE 1 PASS. Lead security/protocol/storage review: APPROVED for this local proof.**

Final integrated build and test run: **70 tests, 70 passed, 0 failed, 0 skipped**, duration 81.660 seconds. All 25 mandatory scenarios are covered, with additional transport, projection, fault and scope tests. This acceptance is limited to Phase 1; it is not Google/firmware/physical production acceptance.

Scope: TypeScript local backend, durable SQL ledger, fake Sheets adapter, recorded/synthetic non-secret fixtures, deterministic failure tests, and documentation. No production ESP32 source/schema change, COM6 access, flash, Google OAuth, real Sheet, credentials, PIN, Wi-Fi or SmartLock state change. Phase 2 has not started.

## Architecture implemented

```text
recorded EventLog CSV fixture
  -> local HTTP/HTTPS ingestion API
  -> PGlite filesystem PostgreSQL ledger
  -> exact row reservations
  -> FakeSheetsAdapter RAW range write/readback
  -> SQL COMMITTED transaction
  -> exact whole-segment ACK on retry
```

The native Node server exposes only `GET /health` and `POST /v1/segments`. The optional standalone runner is loopback HTTPS and requires a test certificate. There is no door-control or local authorization API. Connection provisioning is an internal test/CLI operation, not a public endpoint. It stores only a dedicated ingestion-key hash, not an Owner/browser credential or Google token.

PGlite 0.5.8 identifies itself as PostgreSQL 18.3 on WASM. It provides a real embedded SQL engine with filesystem persistence for this proof. `schema.sql` uses PostgreSQL-compatible relational constraints. One service/worker owns the local database; network PostgreSQL and multi-process deployment are explicitly future work, not an architecture change. Node 24.15.0 was used.

## Relational ledger

| Table | Purpose and integrity constraint |
|---|---|
| connections | Installation/link scope, ingestion-key hash, destination, unit and timezone; one active generation per installation |
| batches | Exact original CSV, protocol, segment ID/hash, ordered event IDs, first receipt time and PENDING/COMMITTED/BLOCKED state |
| deliveries | Unique `(installation_id, link_generation, event_id)`; canonical content/hash, frozen projection version/cells, exact destination/tab/row and delivery state |
| batch_events | Ordered complete batch membership; unique position and event per batch; foreign keys to batch/delivery |
| writer_state | Transactional next-row counters per destination/tab; assigned row starts at 2 |

Reservations and all batch membership commit in one SQL transaction. Conflicting event content or a conflicting segment hash rolls back without consuming more rows. Conflict detection precedes pending-batch admission backpressure. A pending batch occupies one admission slot, but its exact retries remain allowed. Complete archived content/dedup mappings are retained.

## Protocol and ACK contract

One complete closed segment per batch, maximum 8,192 bytes and 32 events. Raw CSV bytes are SHA-256 bound; rows are independently CRC32 validated against the actual production current/legacy format. Protocol rejects malformed UTF-8, schema, timestamps, enums, IDs, nonce mismatch, duplicate/out-of-order sequences, malformed CRC, oversized/truncated data and unsupported metadata. The installation, link generation and dedicated ingestion key determine access to that history destination.

The seven-key COMMITTED receipt contains `protocol`, `state`, `installation_id`, `link_generation`, `segment_id`, `segment_sha256`, and exact ordered `event_ids`. HTTP 200 alone is never the client acceptance rule. ACK validation rejects partial/extra/duplicate/reordered IDs, duplicate JSON keys, wrong binding, malformed data, unsupported state and more than 4,096 bytes. PENDING/202 never authorizes local deletion. No ESP deletion code exists in this phase.

## Reservation and recovery algorithm

Before contacting the fake Sheet, persist exact row assignments and frozen expected cells. Inspect all reserved rows: unexpected occupied content blocks the batch, existing identical content is preserved, and only missing exact ranges are written with RAW semantics. Read back every row and compare every cell before a SQL transaction marks the deliveries and batch COMMITTED. ACK construction additionally verifies every stored delivery state and ordered membership, so setting the batch status alone cannot generate success.

A lost write response or backend termination leaves the original reservation intact. Restart discovers the SQL job, inspects those same ranges and finishes missing work. It never allocates a second row for the event. Five tests terminate a child process forcibly at actual boundaries without graceful database close or an extra test-only filesystem flush: before reservation, after reservation, after Sheet write, before SQL commit, and after SQL commit. A fresh child reopens the filesystem database/fake Sheet and completes; another child restart verifies identical rows and receipt again.

This demonstrates process-crash recovery with the selected embedded engine. It does not certify physical power-loss durability, managed PostgreSQL failover, or real Google behavior.

## Fake Sheets and projection

The adapter exposes only fixed-range read/write plus explicit fixture setup/fault controls. It has no append allocation. Optional external-state persistence is separate from SQL, with flushed temporary-file replacement before lost-response simulation. It models partial writes, quota errors, deletion, edited cells, lost response and an after-write failure. Deterministic row snapshots serve as an independent observable oracle.

Projection has the architecture's exact A-X 24 columns and three Thai tab names. Normal/denied/emergency door events map to door history; supporting/system events map to the system tab; status tab naming is present but a live status dashboard is not implemented. Names/roles absent from recorded history stay absent. Enrollment/revoke identify the subject; verified identity is separate from a claim; physical Admin does not become a named Owner. Legacy denied IDs remain unverified. UTC, backend receipt time and Bangkok display conversion are distinct, and uptime-only events do not acquire invented dates.

Thai text, commas, quotes, newline and formula-like strings are exercised as projection/RAW adapter fixtures. They remain literal data. They are not accepted as an invented firmware CSV schema in Phase 1.

## Fixture provenance

`backend/fixtures/manifest.json` records source and SHA-256. Two segments are transcriptions from existing `evidence/defect_fix_03_09/final-live.txt`, including normal access/unlock/relock, emergency unlock, enrollment, boot/network and denial. These preserve recorded row text, IDs, order and valid CRC; LF delimiters are reconstructed. Original SD files were not accessed, so their byte identity is not claimed. The upload/retry proof binds the checked-in fixture bytes exactly. A clearly synthetic fixture fills denied/revoke/emergency-relock cases absent from the selected capture. No physical PASS is inferred from it.

## Mandatory scenario evidence

Final full command: `npm.cmd test` from `backend`. Detailed output: `evidence/google_phase_01/tests.txt`; machine-readable summary: `evidence/google_phase_01/summary.json`.

| # | Required scenario | Test evidence |
|---|---|---|
| 1 | First normal segment | `service.test.ts` 01/02; real HTTP integration |
| 2 | Exact retry after success | Same receipt and unchanged row snapshot |
| 3 | Concurrent duplicate | 20 submissions plus concurrent worker calls, one row set |
| 4 | Lost ACK | Completed receipt discarded, exact retry returns same ACK |
| 5 | Crash before reservation | Service fault and forcibly killed child, then disk restart |
| 6 | Crash after reservation | Same saved row assignments after process restart |
| 7 | Crash after Sheet write before SQL | Same ranges inspected/reused, missing ranges finished |
| 8 | Partial write | Two rows persisted; PENDING until all nine verified |
| 9 | Correct plus missing rows | Correct row preserved; missing reservations populated |
| 10 | Changed content, same event ID | 409 before/after original commit; no partial SQL writes |
| 11 | Same segment, different hash | 409; original reservation preserved |
| 12 | Wrong installation | Real API/service rejects 403 without cross-scope mutation |
| 13 | Wrong link generation | Real API/service rejects 403; no stale receipt |
| 14 | Out-of-order IDs | Parser and service reject before reservation |
| 15 | Extra ACK event | Exact validator rejects |
| 16 | Missing ACK event | Exact validator rejects |
| 17 | Duplicate IDs | Input ordering/uniqueness and ACK tests reject |
| 18 | Malformed event | CRC/UTF-8/enum/body/schema tests; no partial reservation |
| 19 | Legacy row | Captured legacy segment preserved/projected with unknown names |
| 20 | Quota | PENDING, same reservation, recovery without duplicate |
| 21 | Deleted destination | BLOCKED; no recreation or COMMITTED ACK |
| 22 | Edited reserved row | Conflict before any write, original edit preserved |
| 23 | Lost backend response | Actual HTTP client drops receipt after durable completion; retry matches |
| 24 | Full process restart | Five forced terminations and fresh-process recovery |
| 25 | Ledger reload | Disk reopen preserves bytes/jobs/rows; second restart preserves ACK |

Additional tests cover HTTP route allowlist, duplicate headers, MIME/encoding, 8,192-byte body limits, truncated requests, absolute slow-client deadline, trusted/untrusted local TLS, ownership lock, no partial-state ACK, fixed CRC known vector, malformed ACK keys and RAW projection. No test claims that Google itself supplies exactly-once writes.

## Lead review

The lead owned SQL/schema, service/worker, HTTP boundary, process-crash acceptance tests and final integration. Bounded workers implemented protocol/fixtures, projection/fake adapter and adversarial HTTP tests. The lead reviewed their code and required corrections for UTC seconds versus backend milliseconds, lost legacy identity attribution, revoke-target fixture semantics and serial-transcription provenance. The lead additionally tightened conflict precedence during backlog admission.

An initial integrated run exposed those timestamp integration failures and one test expectation that incorrectly treated an empty header as absent. They were corrected; no failed iteration is reported as a PASS. SQL/state review confirms atomic reservation, exact destination binding, complete-only acknowledgement and safe conflict handling. Source review confirms the backend has no local lock/auth/config mutation capability.

## Boundaries and remaining assumptions

- The fake Sheet models remote persistence and faults; no real Google account, Sheet, quota or OAuth was tested.
- The HTTPS transport test is a local certificate trust test. Production certificates, hosting, rate limits, token vault and OAuth remain future work.
- PGlite proof uses one process/worker per directory; production network PostgreSQL coordination is not certified.
- Managed ranges must not be arbitrarily moved. Unexpected rows are detected, but a human can edit/delete already acknowledged history. Sheets and SQL do not share a transaction.
- Entire-ledger loss requires restore/reconciliation, not automatic reallocation. The runner has a missing-ledger guard; automatic recovery by scanning a real Sheet is not implemented.
- Current schema has no name snapshot. Synthetic snapshot projection tests do not change that fact.
- Full exponential backoff, real provisioning/archives, Google link transitions and production resource tests are later phases.
- No firmware build or physical test was needed or claimed. Production build inputs remain identical to the baseline; previous firmware resource figures remain historical, not remeasured.

## Files and scope preservation

Created `backend/` package (source, SQL, lockfile, fixtures, tests, README and build cleanup) and this report/evidence. The previously authored architecture document is included unchanged in the Phase 1 commit; no architecture contradiction required a revision. Its SHA-256 is `E00BC67590E64D678F0543AA3FB11F1ACD8BD4D3A6D6F279BC1C40DF46730682`.

Unrelated existing edits to `FINAL_FROM_ZERO_REPORT.md`, `STALE_ENROLLMENT_REPORT.md`, and `evidence/final_from_zero/after-phone2.txt` are preserved and excluded from the Phase 1 commit. Generated code, node_modules, local databases and TLS keys are excluded.

## Next phase recommendation

After Phase 1 acceptance, STOP. Phase 2 can separately implement reviewed, versioned local EventLog snapshots/installation metadata and pending-history retention, with host tests and resource measurement before any board change. That phase is not authorized by this implementation pass. No firmware, COM6 or real Google work begins automatically.
