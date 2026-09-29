# Google Sheets removal

OVERALL: PASS

GOOGLE CHECKPOINT: `4439bdbab8ab0260175891b0e45c6e079613f8ce` / `google-sheets-archive-before-removal`

The checkpoint preserves the accepted implementation from `c6c90af967d4bf361047b2e4bb435a0ac132fb37`. It excludes the interrupted, unapproved mobile fix, unrelated historical edits, generated artifacts, and temporary files. The tag is a recovery checkpoint, not a claim that mobile/hardware Google acceptance passed.

GOOGLE PRODUCTION CODE: REMOVED

GOOGLE MANAGEMENT UI: REMOVED

APPS SCRIPT ACTIVE CODE: REMOVED

GOOGLE OAUTH ACTIVE CODE: REMOVED

GOOGLE RECEIVER ENDPOINT: REMOVED

ACTIVE GOOGLE DEPENDENCIES: 0

EXISTING GOOGLE SHEETS: PRESERVED

SD / EVENTLOG: UNCHANGED

LINE SYSTEM: NOT IMPLEMENTED

COM6: NOT ACCESSED DURING THIS CLEANUP

FACTORY RESET: NOT PERFORMED

BUILD: PASS

REGRESSION: PASS

CLEANUP COMMIT: `b6db664e6326eb8d62c45a8317beef6eabf8584c`

## Reviewed cleanup scope

SOL approved removal of the Google sender, pairing and receipt protocols, Google TLS roots and endpoint configuration, Apps Script receiver source/manifest/tests, cloud Management routes and UI, background CloudSync scheduling, Google-only fixtures/probes, and the obsolete cloud preflight. The generic regression runner retains its local suites and drops its removed Google-only test invocation.

The only reset-controller change removes the obsolete `sl-gsync` namespace cleanup reference. No reset was executed. Existing device NVS is untouched; obsolete Google state remains inert on the device until a separately authorized hardware operation. The generic legacy reset cleanup, local identity and PIN authorization, installation metadata, network management, HTTP deadlines, and lock ownership remain intact.

Historical Google architecture, implementation/acceptance reports and evidence are retained under an explicit historical archive index. No remote spreadsheet, Apps Script deployment, OAuth client, Google Cloud project, account grant, or other remote resource was changed or deleted. No local Google fixture process was running when inspected.

## Logging preservation

All EventLog and Audit source remains unchanged, including schema, CRC validation, retention, pending-history protection, segment operations, loss counters and export. The persisted local `google-v1` schema marker and its compatibility tests remain deliberately unchanged. It is an inert serialization marker, not a URL, transport, authorization, or Google dependency. The existing protected-history capacity behavior is retained; this cleanup does not introduce a replacement uploader or change queue policy.

The protected-source comparison confirms 42 files covering events, storage, lock control, access/enrollment authorization, physical Admin and network management are unchanged against the checkpoint. See [preservation evidence](evidence/google_removal/protected-source.json).

## SOL verification

Source-wide search includes Google, Apps Script, script.google, googleapis, drive.file, OAuth, GoogleReceiver, pair_start/pair_poll, spreadsheet and sheet_url. Remaining hits are historical/reference documentation or local log-schema compatibility and its generic tests. No Google connection route, endpoint, sender, OAuth flow or receiver remains in active source. See [classified search](evidence/google_removal/source-audit.json).

The exact production Management page passed desktop Chromium and Pixel 5 Android emulation against isolated synthetic local authorization responses. Dashboard, identities, network, logs and System navigation completed with no JavaScript page errors, no cloud navigation item, and no cloud requests. No existing browser credential/store was accessed or cleared. See [browser evidence](evidence/google_removal/management-browser.json).

An optional older `tests/identity/browser_identity.test.js` harness fails because its fake window lacks `addEventListener`. The same failure was reproduced against the checkpoint before cleanup; production enrollment source is unchanged. This obsolete harness was not repaired as part of cleanup. Current production-code and browser suites provide the regression evidence below. See [baseline comparison](evidence/google_removal/legacy-harness-baseline.json).

## Hardware boundary

This is a source/build cleanup only. The installed board firmware remains unchanged and may still contain the previous Google implementation. No claim is made that the live board has been migrated. COM6, Owner, identities, PIN, Wi-Fi, calibration, SD history and remote Google history were not touched during this cleanup.

NEXT STEP: Await separate LINE notification design instruction. No LINE implementation or hardware deployment is included.

## Build and regression result

All 16 current local regression commands passed: registration/reconciliation, browser credential precedence, QR display timing, Management expiry, HTTP deadlines, EventLog/SD fixtures, CSV export, identity/Owner authorization, core Access/Management smoke, denied-access attribution, physical Admin/PIN/gestures, web PIN authorization, Wi-Fi recovery, request policy/GPIO ownership, and touch state transitions. These are production-method host tests and browser fakes, not physical board acceptance. See [regression results](evidence/google_removal/regression.json) and [test output](evidence/google_removal/regression.txt).

PlatformIO build PASS: flash usage 1,082,617 / 1,310,720 bytes; static RAM 106,496 / 327,680 bytes. Actual application binary: 1,089,200 bytes; binary slot headroom: 221,520 bytes. Binary SHA-256: `9e503605ae434e82e79f4509587f3c441151b6800d42ac7fce89c6e5f6e92e1a`. No Google endpoints, OAuth authorization fields, pairing operations or cloud routes were found in the newly built binary. Only the unchanged local schema marker remains. See [build audit](evidence/google_removal/build-audit.json) and [build output](evidence/google_removal/worker-platformio-build.txt).

SOL FINAL REVIEW: PASS. Full production diff reviewed; Google removals are isolated, generic authorization and logging remain unchanged, and no unrelated historical edits are included in the cleanup commit. No secrets, generated firmware binaries or temporary browser files are staged.
