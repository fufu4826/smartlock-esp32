# Sol post-Astra review — 2026-09-28

## Scope and evidence rule

Sol reviewed the full Astra report, current source/Git, `LINE_WEB_ADMIN_DELIVERY_REPORT.md`, and `CANONICAL_RECURRENCE_INVESTIGATION.md`. This report tracks the current work package. Source, commit, build, flash, saved runtime and human physical acceptance remain separate. Luna's bounded passive diagnostic is complete; no saved report is treated as a fresh board reading.

## Starting Git state

- Repository `C:\ESP\esp32_035_lock_touch_test`, branch `master`, HEAD `8c15375781eab2f54e9d4f573c355ec2243a294b`.
- Latest firmware/source commit `979fc74405944e28375d92bdcb870b5aed1c1f6a`; at the start of this review `src`, `lib`, `platformio.ini` had no tracked diff. Source and test changes made during this review remain uncommitted. `git diff --check` produced no whitespace errors (CRLF warnings were emitted).
- Pre-existing tracked WIP: `CANONICAL_RECURRENCE_INVESTIGATION.md`, `FINAL_FROM_ZERO_REPORT.md`, `STALE_ENROLLMENT_REPORT.md`. Pre-existing untracked phase/evidence files, scripts and tools listed in Astra section 2 remain untouched. Astra's report is also pre-existing untracked content for this Sol run. This file is the new Sol report.
- Existing build artifact and historical deployment reports are not a fresh binary/board provenance comparison. Current live-board image remains unverified.

## C1–C7 disposition

| ID | Sol classification | Decision/evidence |
|---|---|
| C1 | STALE/HISTORICAL | Handoff's nine-file WIP snapshot precedes committed `979fc74`, which contains web LINE maintenance, PIN changes and HTTP release. Saved deployment report claims COM6 flash/preservation. Current live image and latest human acceptance are not inferred from that report. |
| C2 | NEEDS FRESH EVIDENCE | Historical four-event/34,804-byte blocked queue was real at that snapshot. Later saved runtime has 55,284-byte largest block and TLS/response traffic, but three cumulative heap denials and no fresh event push. Neither snapshot establishes the present delivery state. |
| C3 | CONFIRMED | `c5106ac` is receipt documentation; the TLS tag points at `f750bf8`. Documentation attribution only; no firmware action. |
| C4 | CONFIRMED | Protected fallback was introduced at `2b4fcf2`; `c229d41` changes mDNS/modem sleep. Attribution only; current phone recovery path still needs evidence. |
| C5 | CONFIRMED | `AdminPin::begin(configured)` receives a health-derived `configured`; configured can be false on an old install with failed SD/migration/AP-secret validation. If verifier also missing, it can create the initial default. Existing valid verifier is not overwritten. This violates the strict new-device invariant in the fault combination. |
| C6 | CONFIRMED | `safeBootInitialized` includes AP/mDNS/HTTP startup. Physical Emergency Unlock is gated by it even though PIN, SD/auth and LockController safety checks are separate. This is an availability defect under the documented local-authorization requirement; no network failure was induced on hardware. |
| C7 | CONFIRMED | `line-worker` priority 1 and installed Arduino `loopTask` priority 1. No measured starvation. Keep current priority pending causal evidence; core affinity alone does not satisfy the literal lower-priority requirement. |

## R1–R8 classification

| Risk | Classification | Current treatment |
|---|---|---|
| R1: emergency network gate | Security/safety defect requiring correction | Source-proven local availability violation; bounded design decision below. |
| R2: PIN bootstrap conflation | Security/safety defect requiring correction | Source-proven fault path; bounded design decision below. No live PIN data needed. |
| R3: local HTTP/bearer trust | Accepted design limitation / requires user acceptance for any future redesign | Existing product uses trusted local LAN/protected AP and logical browser credentials; no new leak found in the inspected LINE route. Do not start local HTTPS/PKI redesign in this work package. |
| R4: heap and telemetry | Reliability concern / requires more evidence | Existing HTTP release and TLS guard stay unchanged; use fresh passive counters before deciding whether there is a current delivery defect. |
| R5: canonical recovery | Reliability concern / requires more evidence | Saved Windows canonical failure and untested Owner Android/AP workflow remain open; numeric HTTP does not close acceptance. No speculative mDNS/QR change. |
| R6: emitter mutex waits | Reliability concern | Critical sections are short in inspected source, network I/O is outside them, and no deadlock evidence exists. Defer timing change absent measured latency. |
| R7: pre-setup electrical state | Accepted design limitation / requires physical acceptance | Software HIGH at setup and timed relock are preserved; pre-setup magnet behavior cannot be proven by source. |
| R8: LINE task priority | Documentation mismatch / requires more evidence | Same priority is confirmed; no measured starvation or justification for changing scheduling today. |

## Sol design decisions applied in source

**A — Physical emergency gate (R1/C6).** The intended authorization predicate is successful current physical Admin PIN/menu confirmation plus healthy configured local storage, active identity/auth integrity, valid unlock duration, LockController timer readiness and locked GPIO state. AP, STA, mDNS, HTTP, DNS, TLS and LINE readiness are not authorization predicates. The smallest correction is to remove initial AP/mDNS/HTTP success from the latched local emergency gate while retaining all existing local safety and authorization checks, fail-closed storage behavior and timer-before-LOW sequence. Do not modify the web Access path or reset authority. Source-level tests should exercise network startup failures without fabricating a PIN success or hardware result.

**B — PIN bootstrap (R2/C5).** A health-derived `configured=false` is not proof of a new device. Initial default creation is permitted only after affirmative fresh-installation evidence: a healthy unconfigured config, mounted SD, no committed/partial identity authority and no auth verifier/credential store, plus no configured AP/installation evidence. Any ambiguous or corrupt state must fail closed without replacing or creating a PIN. Existing valid verifier must be preserved byte-for-byte; a configured installation with missing/corrupt verifier must not fall back. The implementation may refine these conditions conservatively if tests expose a partial first-setup case, but must return to Sol before broadening authority or changing storage schema. No live secret is needed to test this fault path.

These decisions were applied to source during this review; the resulting firmware has not been flashed. C7 remains an explicit requirement discrepancy; reducing task priority without measured starvation was not approved. R3 is the current trusted-local-network assumption, not a TLS security claim for browser requests.

## Delegated Luna work

1. `luna_passive_baseline` (Luna High): bounded, read-only diagnostic of available live status and provenance; no edits, build, flash, reset, generated event, PIN attempt or secret output. **Completed; Sol reviewed.** COM6 absent and canonical `/health` unavailable, so all live counters/status remain unknown.
2. `luna_passive_baseline` follow-up package (Luna High): implement only C5/C6 under the written decisions above, edit `src/main.cpp`, `src/security/AdminPin.{h,cpp}` and directly relevant host tests only; focused tests and firmware build authorized, flash prohibited. **Status: failed before a diff/result because the agent hit its usage limit; zero implementation was returned.** Sol completed and reviewed the same bounded fixes locally. No Luna implementation work is counted as completed.

## Current baseline and review limits

Saved evidence after `979fc74` reports application-only flash with hash verification and unchanged named NVS namespaces, configured Owner/two identities/two verifiers, locked/GPIO22 HIGH, healthy PIN/SD/calibration, LINE quota TLS activity and 55,284-byte largest heap block. A later saved normal boot/canonical investigation reports LOCKED, STA connected, TLS/responses 36/36 and three cumulative heap admissions blocked with no new event generated. These are **historical software observations**, not current board provenance or proof of physical magnetic force/LINE receipt. `OWNER_VERIFIER_PRESERVED=0` there compares a historical migration backup and does not prove current loss.

Fresh passive collection by Luna: COM6 was absent in the serial/PnP inventory and `\\.\COM6` unavailable, so no serial command was sent. The documented canonical host failed DNS lookup and `/health` timed out; no other address was scanned. The earlier `firmware.bin` metadata was 1,238,000 bytes, SHA-256 `8c3430ea5ba4a542f5e8b1b4a5157833ae5527ef9365ac3d716f987899300c13`; it cannot identify the running image. Boot state, GPIO22, configured/LINE/STA booleans, queue/pipeline counters, free/min/largest heap, admission, mDNS and HTTP readiness are **NOT OBSERVED**. Persistent state preservation at the board is **UNKNOWN**; prior saved comparison is historical.

## Sol implementation and final source review

Because Luna's implementation turn returned only a usage-limit error, Sol made the approved bounded edits directly. `src/main.cpp` now creates an affirmative fresh-install predicate from a newly created config, healthy unconfigured SD, absence of identity/auth/legacy artifacts and absence of prior PIN/installation/AP/STA/LINE NVS keys. NVS key checks are read-only and fail closed on errors or type mismatch. Only that predicate permits `AdminPin::begin` to create a temporary default when the verifier is absent. A valid existing verifier remains untouched even if other health checks fail. The original `initial` marker still supports first-setup continuation with an existing valid temporary verifier.

Physical Emergency Unlock now excludes AP/mDNS/HTTP startup from its latched local gate. It still requires no system fault, configured/healthy local database, available relock timer, locked software and GPIO states, healthy PIN store, a physically authenticated explicit action, current SD identity/auth integrity and a successful timed LockController unlock. The timer remains armed before GPIO22 LOW. No LINE worker, HTTP authorization, reset authority, storage schema, GPIO writer, priority, TLS guard, DNS/mDNS or canonical-origin code changed. Shared LINE secret inputs remain behind the existing Owner+PIN/nonce route; this edit does not add secret output.

Changed source: `src/main.cpp`, `src/security/AdminPin.cpp`, `src/security/AdminPin.h`. Changed tests: `tests/admin/admin_test.cpp`, `admin_transition_test.cpp`, `setup_backend_test.py`. The setup harness previously classified a valid four-digit user-selected PIN as invalid, contrary to unchanged production `FirstOwnerSetup::complete`; that stale expectation was removed. Pre-existing unrelated WIP remains untouched. No Git commit was created.

Verification: Admin focused 93 checks and 10 router/gesture cases PASS; production Setup/AuthStore focused harness PASS; identity suite 349 assertions plus 20 migration fault boundaries PASS; RequestPolicy 43 assertions and GPIO ownership source check PASS; `git diff --check` has no whitespace errors (CRLF warnings only). Final `pio run -e esp32_035` PASS: RAM 112,612/327,680, firmware 1,232,177/1,310,720 bytes; output `firmware.bin` 1,238,752 bytes, SHA-256 `084227e2e77994ae21b773e253c209b8ba49b897354b3969a5f76f328cb06be9`. The host harnesses use isolated fakes and do not prove physical setup, magnet or live LINE delivery. Flash was **NOT RUN** because COM6 is absent; new artifact is not claimed as deployed.

## Unresolved and acceptance

- Current board image/boot slot and current LINE delivery status are unknown.
- Canonical name resolution on the registered Owner Android home-LAN path, and protected AP associated-client canonical recovery, remain unaccepted.
- The original heap incident's current recurrence and three saved transient denials lack a per-denial allocation trace. No TLS guard/stack/priority/DNS/mDNS/origin change is justified.
- Current physical magnet state, Admin PIN web behavior on installed firmware, and the three real-event LINE receipts require separate user confirmation if later acceptance is requested. No event was generated in this passive work package.

**Final Sol verdict: FAIL for end-to-end acceptance.** Bounded source correction and host/build validation PASS, but live-board provenance, deployment, canonical browser acceptance, physical magnet operation and current LINE receipts remain unverified. No destructive operation occurred. State preservation on the live board is UNKNOWN; this work did not intentionally write board NVS/SD/credentials or flash it.
