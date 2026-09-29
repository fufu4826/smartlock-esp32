# Final SmartLock acceptance reconciliation — 2026-09-28

## Verdict

**SOL FINAL PROJECT VERDICT: PASS.** Software acceptance, deployment acceptance and user physical acceptance all pass; project development is complete. The newer user-supplied release closeout explicitly confirms every previously pending physical and canonical-path item. Those are **USER-PHYSICAL-CONFIRMATION**, not inferred from device counters. No physical action was repeated for documentation.

## Production and deployment checkpoint

**SOURCE-VERIFIED:** `master` HEAD `d1e5ce38058790a973ce90c1addf33ef206ce4cd` contains the C5/C6 fix. No tracked `src`, `lib` or `platformio.ini` diff is present. No diagnostic HeapDiagnostics/HeapTrace source is active under `src`. Other documentation/evidence WIP remains untouched. The production source was already committed; no firmware change or new commit was made. Annotated local tag `smartlock-final-accepted-2026-09-28` was created and verified to peel exactly to the accepted commit. It was not pushed.

**DEPLOYMENT-VERIFIED:** accepted production app SHA-256 `084227e2e77994ae21b773e253c209b8ba49b897354b3969a5f76f328cb06be9`, 1,238,752 bytes. The preceding restoration used application-only write to app0 `0x10000`, independent `verify_flash` digest match, then a normal boot of the SmartLock. No subsequent build or flash occurred in the production observation or this closeout package; no diagnostic firmware remains deployed. This combines prior readback with current continuity; the app does not self-report its source commit.

## Acceptance matrix

| Item | Status and evidence |
|---|---|
| Registered Owner scans Access QR | **PASS — USER-PHYSICAL-CONFIRMATION** in the user-supplied release closeout. |
| Owner browser authorization | **PASS — USER-PHYSICAL-CONFIRMATION**. |
| Physical magnetic release on Owner Access | **PASS — USER-PHYSICAL-CONFIRMATION**. |
| Timed physical relock after Owner Access | **PASS — USER-PHYSICAL-CONFIRMATION**. |
| `UNLOCK_SUCCESS` Owner-phone LINE receipt | **PASS — USER-PHYSICAL-CONFIRMATION**. |
| Exactly one intentional wrong physical Admin PIN test | **PASS — USER-PHYSICAL-CONFIRMATION**. |
| Door remained physically locked on wrong PIN | **PASS — USER-PHYSICAL-CONFIRMATION**. |
| `ADMIN_PIN_FAILED` LINE receipt | **PASS — USER-PHYSICAL-CONFIRMATION**. |
| Correct physical Admin PIN / Emergency Unlock action | **PASS — USER-PHYSICAL-CONFIRMATION**. |
| Emergency physical magnetic release | **PASS — USER-PHYSICAL-CONFIRMATION**. |
| Emergency timed physical relock | **PASS — USER-PHYSICAL-CONFIRMATION**. |
| `ADMIN_EMERGENCY_UNLOCK` LINE receipt | **PASS — USER-PHYSICAL-CONFIRMATION**. |
| Registered Owner on intended home Wi-Fi | **PASS — USER-PHYSICAL-CONFIRMATION**. |
| Canonical `smartlock-04225a0ff0a4.local` Owner path | **PASS — USER-PHYSICAL-CONFIRMATION**. |

**SOFTWARE-EVIDENCE:** focused Admin, identity/auth, network, LINE host and GPIO ownership regressions passed in the C5/C6 and minimal-probe work packages. This supports source behavior; the user's statement supplies physical and receipt acceptance. The final no-reset `DIAG_SYSTEM` shows configured/Owner present, SD/calibration/PIN store healthy, lock/GPIO22 locked, STA connected, AP/mDNS/HTTP ready, and LINE configured/enabled/Ready with empty queue. Cumulative counters show five push starts/writes/sends, zero failures and HTTP 200 on the last push. They do not timestamp or identify the confirmed phone receipts. See [final release live status](evidence/heap_minimal_bisection_2026_09_28/final_release_live_status.txt).

## LINE and heap disposition

The interrupted production-only reproduction package observed a real 34,804-byte largest block on the accepted production image with LINE state 5 and one heap denial. Earlier the same boot had 42,996 bytes and two quota responses. The roughly 51-minute intervening history was not continuously captured, so the first transition and exact allocation owner remain unknown. No controlled passive-reset matrix was run before interruption. The final closeout sample shows largest/admission-largest **42,996 bytes**, LINE Ready and successful push writes; heap-blocked remains one. There is no timestamp-aligned sample immediately before any confirmed push. All three named LINE receipts are now **USER-PHYSICAL-CONFIRMATION**. The earlier sub-40 KB state is a **KNOWN REMAINING RELIABILITY RISK**, not an active acceptance blocker in the tested workflow and not a proved fix. The 90,000/40,000 guards, stacks, priorities and network/LINE architecture remain unchanged.

## State, safety and remaining user checks

Latest read-only status: GPIO22 software LOCKED, Owner present, identity/PIN/calibration/SD/STA/AP/mDNS/HTTP healthy, LINE configured/enabled/Ready. User confirmation separately establishes the physical release/relock cases. No source edit, build, flash, reset, Factory Reset, erase, NVS/SD/partition/eFuse change, credential replacement, unlock command, wrong PIN or Emergency action was performed by Codex in this release closeout. Prior accepted app restoration was app-only and preserved observed state; byte-for-byte NVS/SD digests were not obtained in this closeout.

**No mandatory acceptance items remain pending.** Software acceptance: PASS. Deployment acceptance: PASS. User physical acceptance: PASS. State preservation: PASS within the observed read-only health and prior app-only deployment evidence. Destructive operation: NO. Project development complete: YES. The heap reliability question remains documented without reopening the working architecture during release closeout.
