# Sol minimal heap bisection — 2026-09-28

## Starting checkpoint

- Branch `master`, HEAD `d1e5ce38058790a973ce90c1addf33ef206ce4cd`.
- No tracked `src`, `lib` or `platformio.ini` diff at the start; no active `HeapDiagnostics` or `HeapTrace` files under `src`. Unrelated documentation/evidence WIP was preserved.
- Accepted production binary: 1,238,752 bytes, SHA-256 `084227e2e77994ae21b773e253c209b8ba49b897354b3969a5f76f328cb06be9`. Prior restoration verified app0 and a normal boot with GPIO22 LOCKED, Owner/identity/PIN/calibration/Wi-Fi/LINE state healthy.
- Production `.dram0.data` 25,164 bytes, `.dram0.bss` 87,448 bytes, static DRAM total 112,612 bytes.
- The preceding 3,024-byte-BSS diagnostic did not reproduce the production 34,804-byte largest span; it stabilized at 53,236 bytes. This package begins with one stable-idle, near-zero-BSS serial probe before any automatic-stage bisection.

## Minimal stable-idle build gate

The only source change is a fixed serial `DIAG_HEAP_MIN` command in `src/events/Diagnostics.cpp`. It uses one stack-local `multi_heap_info_t`, reads `MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT` and INTERNAL heap values, checks integrity and prints metadata once when requested. It adds no automatic boot probe, task, queue, dynamic allocation or persistent record. Sol reviewed the exact diff before permitting build. Focused LINE, Admin (93 checks and 10 router/gesture cases), security policy (43 checks) and GPIO ownership checks passed; `git diff --check` passed apart from an unrelated pre-existing line-ending warning.

`pio run -e esp32_035` passed. Minimal diagnostic binary: 1,239,904 bytes, SHA-256 `3dcd3bcbd249377712f83e74a2133de94400f55f15ab8073b3a10636a940104d`. Diagnostic `.dram0.data` 25,164 B, `.dram0.bss` 87,456 B, total 112,620 B. Delta versus production: `.data` 0, `.bss` **+8 B**, static DRAM **+8 B**; within the 128 B hard limit. The preserved production backup still hashes to its accepted SHA-256. No flash had occurred at this gate.

## Live stable-idle reproduction gate

The sole CH340 COM6 device was re-identified by PnP and a read-only ESP32 chip probe; its MAC matched the saved accepted record privately (value omitted). The exact minimal binary and patch were preserved under [minimal bisection evidence](evidence/heap_minimal_bisection_2026_09_28/). An app-only write to `0x10000` and independent `verify_flash` digest matched the archived diagnostic image. No other flash region was requested.

After one normal boot and 180 seconds following READY with no UI, PIN, door or LINE Test interaction, the single `DIAG_HEAP_MIN` command returned at `timestamp_ms=182420`:

| Matched capability | Free | Allocated | Minimum free | Largest | Free blocks | Allocated blocks | Integrity |
|---|---:|---:|---:|---:|---:|---:|---|
| INTERNAL|8BIT | 61,536 B | 145,000 B | 8,288 B | **55,284 B** | 19 | 488 | PASS |

Secondary INTERNAL free was 109,464 B and largest was 55,284 B. An existing read-only `DIAG_SYSTEM` followed the heap sample: LINE configured/enabled/Ready, queue/sent/failed all zero, two automatic quota TLS connections/responses, no push, no heap admission denial; its existing `tls_after_largest` counter reported 38,900 B. That historical post-TLS value is not the stable-idle largest block. See [one-shot capture](evidence/heap_minimal_bisection_2026_09_28/minimal_stable_idle_capture.txt).

**Reproduction verdict: NO.** The +8 B image stabilized 20,480 B above the previous production 34,804 B plateau and 15,284 B above the 40 KB guard. Therefore P1, P2, P3 and P4 were **NOT TESTED by design**; no automatic stage probes were built or flashed. No first production-like <=40 KB stage was observed. The result is a non-reproduction under this boot's passive conditions, not proof that the +8 B alone caused the difference. Prior production observations occurred under a different runtime/action history; exact allocation owner and divergence stage remain unknown.

## Production restoration and interpretation

The archived accepted production app (1,238,752 B, SHA-256 `084227e2e77994ae21b773e253c209b8ba49b897354b3969a5f76f328cb06be9`) was restored app-only at `0x10000`; independent `verify_flash` reported digest match. One normal reset reached READY with LockController/GPIO22 LOCKED, identity DB PASS (2 identities, 1 Owner), Admin PIN store and touch calibration healthy, and AP/mDNS/HTTP available. An initial read-only `DIAG_SYSTEM` was taken before STA/time was settled. After a 120-second passive wait, a second **no-reset** `DIAG_SYSTEM` showed `sta=1`, LINE configured/enabled and Ready, queue/sent/failed zero, and two automatic quota TLS responses. The production build's existing admission-largest field was **42,996 B**, not the earlier 34,804 B. This reinforces that the earlier plateau depends on an unreplicated runtime history or non-deterministic allocator placement; the +8 B diagnostic code alone cannot be assigned as its cause. See [restoration summary](evidence/heap_minimal_bisection_2026_09_28/production_restore_summary.txt), [normal boot](evidence/heap_minimal_bisection_2026_09_28/production_restore_live_capture.txt), and [late passive status](evidence/heap_minimal_bisection_2026_09_28/production_restore_late_status.txt).

The exact minimal diagnostic patch, map rows, image SHA and sanitized capture remain archived. `src/events/Diagnostics.cpp` equals HEAD; no HeapDiagnostics or HeapTrace source remains under active `src`. Unrelated WIP was preserved. This package used one diagnostic build and one diagnostic flash; P1–P4 and further probes were not run. No Factory Reset, erase_flash, NVS/SD/partition/otadata change, credential replacement, physical lock action or other destructive operation occurred. Production LINE delivery remains unaccepted; no real notification receipt was tested.

**Next method:** first establish a repeatable 34,804 B production condition with a controlled, identical boot and runtime/action history, using existing read-only output and no new firmware. Only then consider external debugger/JTAG heap inspection if the actual board exposes usable pins and an adapter/toolchain is verified. Do not continue source bisection or implement a production fix until that baseline exists.
