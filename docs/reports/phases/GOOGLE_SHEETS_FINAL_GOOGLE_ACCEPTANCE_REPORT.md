HISTORICAL: Google Sheets is removed from the active product. This document records an earlier design or test checkpoint and is not current implementation authority.

# Final Google acceptance — 2026-09-27

OVERALL: PASS — the two final Google acceptance gaps were closed in the follow-up below. Earlier incomplete-run evidence is preserved for traceability. Hardware deployment requires separate authorization; COM6 remains untouched.

## Final gap-closure review — 2026-09-27

### Management consent navigation: PASS

Final disconnected host fixture: loopback port 60859, serving the production `kManage` JavaScript unchanged. SOL clicked its Connect Google Sheets button once. The browser itself created tab 6 and navigated it to Google's OAuth account/permission flow. No authorization URL was copied, extracted, supplied to a browser navigation tool, or manually reopened during this final run. Google then returned the successful Thai callback. Without reloading Management or clicking Refresh, Management changed to connected and displayed the account and automatically provisioned [SmartLock 61b7b061](https://docs.google.com/spreadsheets/d/<redacted-sheet-id>/edit).

The previous popup observation was a browser-host visibility/control problem, not an established production JavaScript defect. Two additional fixture mismatches were found and corrected: structured transient receiver errors and transport failures had been exposed directly to the browser, while production CloudSync retains its pending pairing state and retries. The corrected fixture preserves PENDING and its validated Google URL. No WebAssets or other production firmware edit was made for this gap closure.

The strengthened ordinary Chromium test requires navigation beyond about:blank, visible consent content, null window.opener, a simulated user consent/callback, a transient transport error, and eventual connected Management. It passed. The live final run independently confirmed actual Google callback and automatic connected status. Browser VM regression: 48 checks passed. This is Google acceptance using a host device boundary, not ESP32 hardware acceptance.

### Controlled real Google destination failure and recovery: PASS

Only the newly provisioned disposable [SmartLock ba154d7c](https://docs.google.com/spreadsheets/d/<redacted-sheet-id>/edit) was used. The previously accepted SmartLock 0ca1f00a history Sheet was untouched.

1. Prepared one valid authenticated event in process memory without sending it. The same serialized bytes were retained throughout.
2. Temporarily renamed the disposable Sheet's access tab from `ประวัติการเข้าใช้งาน` to `temporary_failure_access`; Google visibly confirmed the change was saved. No cells or files were deleted.
3. Sent the event three times while that required named range was unavailable. Each response was ERROR/busy_retry, with no valid seven-field completion receipt. Pending remained true.
4. Restored the exact original Thai tab name and verified Google saved it.
5. Retried the same bytes: exact COMMITTED receipt. Retried again: exact ALREADY_STORED receipt. Live Sheet inspection showed one event row, with no duplicate row.

Event ID: `567bd91efc7e4c3a41717b2a21c0979c00000001`.

Segment SHA-256: `eb09e70e3c66fd72ca6261757f44b32ca4b8f7b21df425da6646d8f27caa8ba1`.

Interpretation of the error is based on the controlled destination change, unchanged event/proof, repeated failures, successful restoration, and source review—not on its label alone. The receiver's `withLock()` catches generic exceptions from its callback, including Google API failures, and maps them to `busy_retry`. Thus that label does not distinguish actual lock contention from Google failures. The fixed named-range Google read fails when the tab is absent; restoring only that tab allowed exact readback and ACK. The test does not claim a captured raw Google HTTP response or a global Google outage.

The first disposable trial, SmartLock aaae47ff, also had its original tab restored and received COMMITTED then ALREADY_STORED. All renamed disposable tabs are restored. No permission was revoked, no unrelated grant was changed, no quota was exhausted, and no accepted history was deleted. Disposable artifacts are retained as evidence rather than permanently deleted.

### ACK, regression and final approval

The fixture retained exact bytes until a matching ACK; five focused fixture tests passed, including repeated failure and resumable duplicate checking. Production CloudSync's 44 checks passed again: HTTP 200 and error bodies cannot retire SD data, and exact ACK plus unchanged connection/segment remains required. Actual board SD retirement was not exercised and remains a later hardware check.

Apps Script stays version 4 with the previously recorded identical source SHA-256. No cloud deployment, OAuth scope, permission, billing or production source change was made in this gap-closure run. Billing remains disabled under the already verified unlinked project configuration. No remote door-control route was introduced. The only new/updated implementation files in this run are host fixture/tests and their documentation. Installation proofs and OAuth tokens were not added to evidence files or Sheet cells.

SOL reviewed the fixture corrections, real destination failure, recovery receipts, exact ACK retirement gate and unchanged production behavior. The two requested Google acceptance gaps are closed. Previous incomplete results below are historical, not the current release decision.

Saved non-secret evidence under `docs/phase_reports/evidence/google_one_click/`: `final-gap-management-connected.png`, `final-gap-recovered-single-row.png`, and `final-gap-recovery-receipt.txt`. The two live fixture processes used for final evidence were stopped after capture; their synthetic proofs existed only in process memory. No background upload was left running from these fixtures.

**GOOGLE ACCEPTANCE: PASS**

**HARDWARE DEPLOYMENT: READY FOR SEPARATE AUTHORIZATION**

No COM6 access or flash, Factory Reset, local Owner/identity/PIN/Wi-Fi/SD/calibration mutation, or Git commit was performed.

## USER ACTIONS

1. Open SmartLock Management.
2. Tap Connect Google Sheets.
3. Sign in to Google and grant permission.

These are the required product actions. The actual acceptance run required Codex to reopen the pending Google authorization page because the in-app popup was not reliably visible. Therefore this report does not claim the required flow passed without assistance.

## Deployment and configuration

- Apps Script version 4 was deployed to the existing endpoint; no new backend was introduced.
- Deployment ID: `<redacted-apps-script-id>`.
- Reviewed `cloud/google_receiver/Code.gs` SHA-256: `a9784a7d718bce7e9969c4f943deb6ef7a129d053504909f23ce78d300eb6b08`.
- Publisher: <redacted-email>. Execution identity remains the publisher; receiver HTTP requests are public but authenticated operations require installation proofs.
- OAuth project: `bright-petal-509909-c6`. After explicit user confirmation, Audience visibly changed from Testing to **In production**.
- OAuth scopes remain openid, email and drive.file. Apps Script manifest uses external_request. No full Drive or Sheets account-wide OAuth scope was added.
- Public Thai About and Privacy pages were deployed and opened successfully. Branding links saved successfully. Production status is not a claim of completed Google brand verification.
- Google Cloud Billing page explicitly showed no linked billing account. No trial, card, payment, purchased domain or paid quota was activated. Cloudflare, D1 and PostgreSQL are not part of the active receiver architecture.

## Fresh consent and automatic provisioning

The fresh process used production `kManage` HTML/JavaScript from WebAssets.h, with a loopback-only synthetic device API boundary and random installation proofs kept in process memory. It did not authenticate against the physical ESP32 or access its storage. Only the fixture origin received synthetic browser credentials; existing browser storage was not cleared.

The first stale pending fixture was stopped; a new disconnected fixture was started. Its Connect action produced a real pending Google authorization request. Due to in-app popup visibility/control problems, Codex reopened that same pending authorization URL. The user selected their Google account and granted permission. The callback displayed the Thai success message. Pair polling returned COMMITTED; the fixture then exposed connected status to the unmodified production Management JavaScript.

Management visibly showed connected, the intended account, the automatically generated spreadsheet title, and an Open Google Sheets link. No Sheet ID, API key, endpoint or token was requested from the user.

Fresh staging spreadsheet: [SmartLock 0ca1f00a](https://docs.google.com/spreadsheets/d/<redacted-sheet-id>/edit).

The Sheet was private to the account and had exactly these visible tabs, without an extra localized default tab:

- ประวัติการเข้าใช้งาน
- เหตุการณ์ระบบ
- สถานะระบบ

The visible access headers and rows were inspected in Google Sheets. Thai text with comma and quotes was read from C2 as `ทดสอบ, "ภาษาไทย"`. C3 displayed `=1+1`, and the formula bar showed the protected literal form, not an evaluated result of 2. Full live enumeration of every hidden technical header and both other tabs' headers remains unrecorded; provisioning host tests cover their creation.

Repeated committed pair polls recovered the existing connection. Lost-callback and duplicate-provisioning protection is covered by host tests; this run did not inventory Google Drive to prove globally that no duplicate spreadsheet exists.

## Real event acceptance

`tests/cloud/management_live_fixture.py` sent synthetic events to the actual version-4 receiver. No real door history was uploaded by the fixture.

| Check | Result | Evidence |
|---|---|---|
| First two-event upload | PASS | Exact seven-field COMMITTED receipt matched installation, generation, segment digest and event IDs |
| Identical retry | PASS | Exact ALREADY_STORED receipt |
| Conflicting same event ID | PASS | CONFLICT, no valid completion receipt |
| Thai UTF-8, comma and quotes | PASS | Exact receiver readback plus live Sheet C2 inspection |
| Formula-looking string | PASS | Literal Sheet cell, not evaluated formula |
| Malformed JSON | PASS | ERROR |
| Wrong installation proof | PASS | REAUTH_REQUIRED |
| Lost successful response | PASS, simulated loss against live receiver | Fixture intentionally discarded the successful response and retried identical bytes; exact ALREADY_STORED receipt |
| Duplicate row count | PASS for tested batches | Three visible history rows: two original events and one lost-ACK event, without retry duplicates |
| Live Google API failure | NOT TESTED | No Google outage, quota exhaustion or account revocation was induced |

Do not relabel the simulated dropped response as a physical network outage. Google failure/no-ACK behavior is covered by dependency-failure host tests only. A proposed temporary header corruption was rejected after source review: header validation occurs during provisioning, not every segment upload. No Sheet data was changed for that proposal.

## Host verification and review

Verified in this acceptance work:

- Receiver and fixed publisher-page tests: 25 passed, 0 failed.
- Production web-route checks: 43 passed.
- Production Management JavaScript VM checks: 48 passed.
- Protocol assertions: 63 passed.
- Production CloudSync pairing/ACK/SD-retirement checks: 44 passed.
- Loopback Management fixture self-test: passed, including guarded test route and offline start/poll flow.

These are 223 named checks/assertions across suites plus the fixture self-test, not 223 independent physical or end-to-end tests. No unrelated historical suite was rerun.

SOL reviewed receiver routing, fixed public-page dispatch, callback result handling, scoped OAuth configuration, device proof checks, fixed-row dedup/readback, and exact ACK semantics. Luna implemented bounded fixture/public-page work; SOL identified and required correction of the fixture test-route authorization ordering and sandbox-relative public-page links before acceptance use.

Receiver operations are pair_start, pair_poll, segment, status and disconnect. There is no receiver route for lock, unlock, PIN, Owner, local identity, Wi-Fi, Factory Reset or firmware mutation. Audit action labels are data, not executable commands. Google tokens remain in publisher Script Properties, not on ESP32 or in Sheet cells. Installation proofs are POST fields, not spreadsheet data. No secret properties were printed for this review.

HTTP 200 alone cannot retire local data. Production CloudSync tests require exact protocol/state/installation/generation/segment/digest/event-ID matching, unchanged current connection state and an unchanged local segment before retirement. Invalid, conflicting or uncertain responses do not authorize retirement.

## Earlier incomplete-run blockers (superseded by final gap-closure review)

1. Demonstrate the production Management Connect action reaching visible Google consent without Codex reopening a URL. Diagnose browser-host popup handling separately from product behavior; do not claim a firmware defect solely from the in-app automation behavior.
2. Complete recorded live header enumeration and a controlled Google API failure/no-valid-receipt test if retaining the requested all-live acceptance standard. Do not damage existing history or revoke unrelated grants to manufacture this test.
3. The tested Management boundary is a host fixture, not a running ESP32. Real ESP32 transport, heap, local SD retirement and physical operation remain for separately authorized hardware acceptance after the Google gate closes.

COM6 was not accessed or flashed. Owner, identities, PIN, Wi-Fi, SD history, calibration and Factory Reset were untouched. No commit was created for this acceptance report.

Earlier result: GOOGLE ACCEPTANCE: FAIL — incomplete end-to-end UX/live-failure evidence, despite successful Google provisioning and upload checks. Superseded by the final PASS above.

Earlier hardware gate: not yet ready. Current gate: ready for separate authorization; do not flash automatically.
