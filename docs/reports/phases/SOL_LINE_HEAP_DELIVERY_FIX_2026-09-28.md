# Sol LINE heap delivery investigation — 2026-09-28

## Starting state and scope

- Branch `master`, HEAD `d1e5ce38058790a973ce90c1addf33ef206ce4cd`; deployed application SHA-256 `084227e2e77994ae21b773e253c209b8ba49b897354b3969a5f76f328cb06be9`, verified on app0 in the preceding work package.
- No new tracked source/lib/platform diff at start. Pre-existing modified tracked phase reports and untracked architecture, reports, evidence and tools remain untouched. This report is new for this work package.
- Baseline postflash `DIAG_SYSTEM`: free heap 109,552, minimum 55,268, largest contiguous block 34,804 bytes; LINE heap-blocked 214, queue 0, generated/enqueued event counters 0; LINE configured/enabled, STA connected, AP enabled with zero clients, mDNS/HTTP ready. This is a current admission-risk signal, not a generated-event delivery test or a proved cause.
- The TLS admission policy remains free heap >= 90,000 and largest block >= 40,000. No guard or priority change is approved.

## Passive measurement and root-cause review

Luna High completed one read-only controlled matrix on the currently deployed application, saved as four validated JSON files in `evidence/line_heap_2026_09_28/`. The board stayed configured, STA-connected, locked/GPIO22 locked, with healthy local stores. No secret, PIN, unlock, enrollment, LINE Test or reset was used.

Seven AP-on idle samples from 17:03:17–17:06:21 local showed free/min/largest heap fixed at 109,336/55,268/34,804 bytes. The LINE queue, generated events, push starts, TLS connections and responses did not change; `heapBlocked` rose 16,818→17,002 (+184, about 60/minute). A canonical `.local` GET failed at the local resolver and never reached the board. One direct STA `/health` GET with the canonical Host header returned HTTP 200, causing the existing no-client AP-off transition after 3.25 seconds. Free heap rose from 109,336 to 111,888 bytes, but the largest block remained 34,804 and blocked admissions continued. One unauthenticated `/manage` GET and one inert `/a/<dummy>` GET returned 200 without running JavaScript, authentication or unlock; neither changed the largest block. Seven AP-off idle samples from 17:10:17–17:13:21 showed free heap recovering to 112,144, min unchanged at 55,268 and largest fixed at 34,804; `heapBlocked` rose 17,238→17,421 (+183, about 60/minute). TLS connections/responses remained 2/2 and queue/event/push counts remained zero. These GETs do not represent a full authenticated Owner browser session.

**Sol classification: PERSISTENT_ALLOCATION / HEAP_FRAGMENTATION observed; exact allocation owner UNKNOWN.** The AP-on state, one AP transition and these two simple page loads are not sufficient causes of the persistent 34,804-byte contiguous limit. The monotonic blocked counter is explained by the LINE worker's once-per-second `heapAdmitted()` retry while largest is below 40,000; it does not represent 17,000 new notification attempts. No measured TLS peak/safe margin exists to justify lowering the 90,000/40,000 guards. No source-level root-cause fix is approved.

## Worker limit and final status

Luna started a bounded diagnostic-only `DIAG_HEAP` source edit but hit a usage limit before completing the requested instrumentation/build or returning measurements. Sol reviewed the partial diff and removed only those incomplete edits; current `src`, `lib` and `platformio.ini` have no diff from `d1e5ce3`. Luna completed one passive matrix task; its instrumentation follow-up failed. No new build artifact, fix commit, flash, image verification, post-fix heap matrix, LINE Test, API push or Owner phone receipt exists in this work package. The existing deployed C5/C6 image remains the last verified image. No destructive operation occurred.

**Sol verdict: FAIL for LINE delivery restoration.** Root cause is not sufficiently isolated for a safe fix; the last observed largest block is 34,804 bytes, below the unchanged 40,000-byte TLS admission threshold, with `heapBlocked` still increasing. Persistent state and lock safety were observed healthy during the read-only matrix but no new deployment or full physical acceptance was performed. Physical door acceptance is not ready while LINE transport remains unproven. This bounded work package stops at the worker usage limit as instructed.
