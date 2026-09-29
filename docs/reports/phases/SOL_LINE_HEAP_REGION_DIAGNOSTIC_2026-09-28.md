# Sol region-first heap diagnostic — 2026-09-28

## Verdict

**Diagnostic capture: PASS. Exact production root cause: NO.** The diagnostic image did not reproduce the production 34,804-byte largest-block plateau or a denied LINE admission. It stabilized at 53,236 bytes. Its +3,024-byte static DRAM footprint and different behavior mean the observer/layout effect is material. The LINE worker creation caused the largest measured task-stage drop (81,908→59,380 bytes), then first quota TLS teardown reduced it to 53,236. These implicate task placement and TLS allocation interleaving as contributors in the diagnostic image, not a proven owner of the production plateau. No production fix was attempted.

## Starting checkpoint and orphan source

- Branch `master`, HEAD `d1e5ce38058790a973ce90c1addf33ef206ce4cd`; unrelated documentation/evidence WIP preserved. Accepted production app: 1,238,752 bytes, SHA-256 `084227e2e77994ae21b773e253c209b8ba49b897354b3969a5f76f328cb06be9`. Production `.dram0.data` 25,164 B, `.dram0.bss` 87,448 B, total 112,612 B.
- Sole CH340 COM6 adapter had the prior PnP instance; read-only ESP32 chip probe matched the saved board MAC privately (value omitted). Chip: ESP32-D0WD-V3 rev 3.1, 40 MHz.
- Untracked `src/events/HeapTrace.cpp/.h` were incomplete interrupted-diagnostic residue (aggregate 8BIT only, eight phases, no production call sites). They were hash-preserved under `evidence/heap_diagnostic_orphan/` and moved out of `src` before build. SHA-256: cpp `ca5214336a6a0d7f7eb382af793e24d44f2927f0d981f6f56a31dacd9399e634`; h `f4e1381971540b68ff0e051094a87d8d23d6ce8535a2de0ff3d962d4c133b4b5`.

## Diagnostic design, build and deployment

The image used 24 static 124-byte records (2,976 B) plus 48 B fixed state. Each retained phase/time/sequence; INTERNAL, 8BIT, INTERNAL|8BIT and INTERNAL|DMA free/allocated/minimum/largest/block counts; PSRAM availability; and known task watermarks. `DIAG_HEAP` printed records after sensitive windows, one metadata-only per-region summary and an idle INTERNAL|8BIT integrity result. No credential, payload, HTTP diagnostic, GPIO write, guard, task setting, PIN/auth, network setting, storage schema or initialization order changed. The patch and new files are preserved under [diagnostic evidence](evidence/heap_diagnostic_2026_09_28/) and removed from active production `src`.

`pio run -e esp32_035` passed. Diagnostic app: 1,242,352 B, SHA-256 `06971003d545542d1f9975f62104319ebafc9d3560d6be97cf2c2ab0e7f259f9`; flash use 1,235,781/1,310,720 B. `.dram0.data` 25,164 B, `.dram0.bss` 90,472 B, total 115,636 B: **+3,024 B DRAM** versus production. LINE host tests, Admin 93 checks and 10 router/gesture cases, setup backend, RequestPolicy 43 checks, GPIO ownership policy and `git diff --check` passed. The only diff-check warning concerned pre-existing unrelated line endings.

The app-only diagnostic write and independent `verify_flash` at `0x10000` passed. One normal boot produced all phase records, but its filter discarded plain-text region output. Sol classified it incomplete and authorized exactly one corrective normal boot. The corrected sanitized capture includes region metadata. No physical door, UI, PIN or LINE Test action occurred.

## Corrective boot phase measurements

All quantities are bytes except block counts and milliseconds. Full four-capability tuples (free/allocated/minimum/largest/free blocks/allocated blocks) appear in [the corrective serial capture](evidence/heap_diagnostic_2026_09_28/serial_diag_capture_corrective.txt). No PSRAM was present. Small differences between sequential capability calls at some boot phases reflect concurrent allocations.

| Phase | ms | INTERNAL free | INTERNAL|8BIT free | largest | free blocks | allocated blocks | LINE HWM |
|---|---:|---:|---:|---:|---:|---:|---:|
| H00 | 131 | 232,152 | 184,264 | 110,580 | 11 | 110 | — |
| H01 | 141 | 230,248 | 182,360 | 110,580 | 11 | 125 | — |
| H02 | 1,018 | 230,016 | 182,128 | 110,580 | 11 | 128 | — |
| H03 | 1,253 | 202,304 | 154,416 | 86,004 | 12 | 139 | — |
| H04 | 1,721 | 202,304 | 154,416 | 86,004 | 12 | 139 | — |
| H05 | 1,722 | 202,304 | 154,416 | 86,004 | 12 | 139 | — |
| H06 | 2,002 | 149,976 | 102,088 | 86,004 | 13 | 293 | — |
| H07 | 2,008 | 148,016 | 100,128 | 86,004 | 11 | 302 | — |
| H08 | 2,013 | 142,016 | 94,032 | 86,004 | 9 | 335 | — |
| H09 | 2,023 | 135,428 | 87,540 | 86,004 | 10 | 452 | — |
| H10 | 2,201 | 133,324 | 85,396 | 81,908 | 12 | 463 | — |
| H11 | 2,206 | 133,300 | 85,316 | 81,908 | 12 | 464 | — |
| H12 | 2,207 | 133,544 | 85,508 | 81,908 | 13 | 460 | — |
| H13 | 2,212 | 133,204 | 85,180 | 81,908 | 14 | 464 | — |
| H14 | 2,213 | 108,120 | 60,132 | 59,380 | 10 | 471 | 19,524 |
| H15 | 2,214 | 107,960 | 60,072 | 59,380 | 11 | 472 | 19,524 |
| H16 | 21,752 | 106,864 | 58,976 | 55,284 | 15 | 482 | 19,524 |
| H17 | 21,754 | 106,864 | 58,976 | 55,284 | 15 | 482 | 15,812 |
| H18 | 25,864 | 106,368 | 58,480 | 53,236 | 15 | 490 | 11,444 |
| H19 | 25,866 | 106,368 | 58,480 | 53,236 | 15 | 490 | 11,444 |
| H20 | 29,939 | 106,360 | 58,472 | 53,236 | 16 | 490 | 11,444 |
| H21 | — | — | — | — | — | — | unavailable: no denied admission |
| H22 | 45,143 | 106,400 | 58,512 | 53,236 | 15 | 489 | 11,444 |

First material largest-block drop: H02→H03, 110,580→86,004, during storage initialization. Largest task-related drop: H13→H14, 81,908→59,380, across LINE worker creation. Quota #1 H17→H18: largest 55,284→53,236; INTERNAL|8BIT free 58,976→58,480; allocated 144,632→145,008. Quota #2 H19→H20: largest unchanged at 53,236; free 58,480→58,472; allocated 145,008→145,016. The first TLS exchange did not permanently retain tens of kilobytes in this image. Production 34,804 was **never observed** in either diagnostic boot.

At H22, 8BIT and INTERNAL|8BIT were effectively equal: free 58,512, allocated 144,992, minimum free 2,472, largest 53,236, 15 free and 489 allocated blocks. INTERNAL was free 106,400, allocated 144,992, minimum free 50,340, largest 53,236, 16 free and 489 allocated blocks. The 47,888 B free difference is material: the mixed-capability production admission comparison cannot by itself establish TLS-eligible capacity. It does not justify changing either guard.

## Region, integrity and stack

One idle `heap_caps_print_heap_info(INTERNAL|8BIT)` listed nine regions. The dominant region at `0x3ffe4350`, length 113,840 B, had free 58,348 B, allocated 54,304 B, largest free 53,236 B, 9 free and 19 allocated blocks. Each other region had 0–24 B free and no meaningful large span. Printed totals: free 58,412 B, allocated 145,076 B, minimum free 2,472 B, largest 53,236 B. These follow H22 and differ slightly from its snapshot. Dominant-region free minus largest was 5,112 B across small spans. This diagnostic state is dominated by one >40 KB contiguous span; production intra-region fragmentation remains unproven.

H22 `heap_caps_check_integrity(INTERNAL|8BIT, false)`: **PASS**. LINE worker configured stack 24,576 B; observed low-water free margin 11,444 B after quota TLS (about 13,132 B maximum used in this run). The bundled Arduino ESP32 sdkconfig sets loopTask stack to 8,192 B; its observed low-water free margin was 4,188 B (about 4,004 B maximum used). Local ESP-IDF headers confirm HWM units are bytes. No stack size changed.

## Production restoration and state

The preserved accepted production binary was app-only flashed to `0x10000`; independent `verify_flash` digest matched its 1,238,752-byte SHA-256 `084227e2e77994ae21b773e253c209b8ba49b897354b3969a5f76f328cb06be9`. After one normal reset the board reported BOOT, LockController/GPIO22 LOCKED, configured/Owner present, identity DB PASS (2 identities, 1 Owner), SD/calibration/Admin PIN and installation stores healthy, AP/DNS/mDNS/HTTP/STA healthy, and LINE configured/enabled with empty queue. Initial LINE state was TimeUnavailable; a later no-reset read-only check showed Ready. Evidence: [restore capture](evidence/heap_diagnostic_2026_09_28/production_restore_capture.txt) and [later status](evidence/heap_diagnostic_2026_09_28/production_restore_later_status.txt). No physical unlock or real LINE receipt was tested.

Only the four tracked diagnostic source targets were restored to HEAD and the two new HeapDiagnostics files removed from `src` after preserving patch/copies and checking hashes. No diagnostic source remains under `src`; unrelated WIP remains. No Factory Reset, erase, NVS/SD/partition/otadata change, credential replacement, physical lock action or other destructive operation occurred.

**Next action:** separately design a smaller, production-representative measurement to identify why the accepted image reaches 34,804 B. Preserve the 90,000/40,000 guards and task/network settings until that evidence exists. Do not implement a fix in this diagnostic package.
