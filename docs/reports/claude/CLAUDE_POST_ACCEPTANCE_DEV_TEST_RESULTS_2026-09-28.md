# Post-acceptance development: test and build results (2026-09-28)

These results cover prototype development **after** the accepted release (`smartlock-final-accepted-2026-09-28` → `d1e5ce3`, which is unchanged).

- All results are SOFTWARE-EVIDENCE: host tests and an offline build.
- No real-board acceptance has been performed for these commits.
- Design details: `03_CHAPTER_3_SYSTEM_DESIGN_AND_DEVELOPMENT/CLAUDE_MANAGEMENT_WEB_SETTINGS_IMPLEMENTATION_2026-09-28.md`.

| Commit | Change | Host tests | Build | Firmware SHA-256 | Board acceptance |
|---|---|---|---|---|---|
| `523fd0b` | Factory Reset preserves the prototype LINE config (`sl-line`); `suspendForReset`; C5 fresh-install excludes `sl-line` | PASS | PASS: flash 1,232,401 B (94.0 %), RAM 112,612 B | `bbbba22ab192a41301646cf0149486c0fa58e17eea400737deca77ae4da1cc7d` | PENDING |
| `a5325b5` | Owner Management: merged LINE page with Add Friend, unlock-duration page and API, dedicated Admin PIN change page | PASS | PASS: flash 1,239,785 B (94.6 %), RAM 112,612 B, bin 1,246,368 B | `d208be3596feda245e584dcb034ec210132565b79528d57c2a94e5c3bfdc621a` | Run on board; not tagged. Superseded because notifications reached only the old fixed recipient. |
| `eaac772` | LINE delivery changed to broadcast to all OA friends; recipient no longer required; v2 config read unchanged | PASS (including `line_maintenance_test.py`) | PASS: flash 1,239,813 B (94.6 %), RAM 112,612 B, bin 1,246,384 B | `546e96b03583eaf5405bbd7b1a6100cfee91937ba9047cc0026db4f4caa58e36` | PENDING |

## Suites for `a5325b5`

| Suite | Result |
|---|---|
| Admin host (AdminPin, PhysicalAdmin, LockController, ConfigStore) | 116 checks PASS, plus 10 router/gesture cases PASS |
| Admin web source/behaviour (`web_test.js`) | PASS |
| Management cleanup/navigation (`cleanup_ui.test.js`) | PASS; its stale navigation assertion was updated |
| Mobile Playwright Management flow (390×844) | 8 checks PASS |
| Setup browser (`setup_browser_test.js`) | PASS |
| LINE host (suspend, preserve, reboot) | PASS |
| Factory Reset LINE-preservation contract | PASS |
| Security policy (43) and GPIO22 ownership | PASS |
| Identity (349) and registration | PASS |
| Network manager (155) and canonical origin (20) | PASS |
| HTTP deadline and touch state | PASS |
| Event log, phone2, defect-fix regression | Pre-existing stale failures on the base (retired `EventLog.h`; harness ``'network' undeclared``); not regressions |

## Pending real-board items

1. Flash the application only to app0 (`0x10000`) and run `verify_flash`.
2. LINE page shows Ready and the Add Friend button.
3. Unlock-duration change takes effect on the next Owner Access; the user confirms release and relock.
4. Admin PIN change works on the TFT, and the old PIN is rejected.
5. Factory Reset preserves LINE, fresh Setup works with no C5 fault, LINE is Ready without reprovisioning, and the user confirms receipt of a LINE Test.
