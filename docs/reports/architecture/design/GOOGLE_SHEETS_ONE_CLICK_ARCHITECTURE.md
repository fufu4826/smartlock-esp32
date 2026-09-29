HISTORICAL: Google Sheets is removed from the active product. This document records an earlier design or test checkpoint and is not current implementation authority.

# SmartLock Google Sheets: one-click architecture

SOL decision, 2026-09-27. Supersedes the Node/PostgreSQL and Workers/D1 designs. No hardware deployment is authorized.

**Current status (2026-09-27):** Final Google owner-flow and receiver acceptance is PASS in the [final acceptance report](../phase_reports/GOOGLE_SHEETS_FINAL_GOOGLE_ACCEPTANCE_REPORT.md). A separate user authorization now covers non-erasing COM6 deployment under hardware phase H1; deployment and on-board validation are still pending in the [H1 request and evidence](../phase_reports/evidence/google_h1/). This status note records later authorization without changing the original G0 design decision; it does not claim that firmware has been deployed.

## G0 decision

**YES** — an owner can use a preconfigured integration without opening Cloudflare, Google Cloud Console, Apps Script editor, or spreadsheet setup. Use one publisher-operated Google Apps Script web application with server-side Google OAuth and the non-sensitive `drive.file` scope. This is architectural approval, **not** a claim that provisioning or the real Google acceptance test has passed.

A developer OAuth application and deployment must exist first. Codex provisions those once using the authorized publisher account. They are not per-owner setup. No billing/trial/card is attached. A Google security challenge, publisher approval, or account restriction cannot be bypassed or silently turned into an owner setup instruction. If provisioning cannot finish, report the precise blocker and leave synchronization disabled. Public app branding/verification is a separate publisher gate; do not promise a verified public product based on personal-account testing.

## 1. Exact owner flow

Management → ระบบ → Google Sheets → เชื่อมต่อ Google Sheets → Google sign-in/consent → Done. No second local confirmation, pasted identifier, developer setting, or intermediate authorization-link button. Open the consent window synchronously on the original click; navigate it once the local Owner-authorized pairing request is ready. The Management page polls for completion.

## 2–4. Components, ownership, provisioning

```text
Owner browser ── local Owner Management authorization ── ESP32
     │ Google consent                                    │
     ▼                                                   │ HTTPS POST
Google OAuth ── callback ── Apps Script receiver ◀─────────┘
                                  │ owner OAuth grant
                                  ▼
                         Owner's Google spreadsheet
```

Publisher: one Apps Script project, one web deployment executing as publisher, one standard Google project/OAuth web client, Sheets and Drive APIs. Codex configures the exact Google-hosted callback and stores the OAuth client secret only in Script Properties. Apps Script needs external-request capability; it calls Google REST APIs using the consenting owner's token, not the publisher's Drive permissions. No automatic file sharing with the publisher, Cloudflare, SQL database, or external history ledger.

## 5–7. Consent, provisioning, destination

Use authorization-code OAuth, offline access, `drive.file`, `openid` and email identity scopes only. Google access/refresh tokens stay server-side in publisher-controlled Script Properties. Never send them to ESP32 or browser storage. Register the same deployed `/exec` URL as the exact Google OAuth callback; its `doGet` handles the code under the deployment's publisher identity. Validate signed, expiring, single-use state before exchanging a code. This avoids a separate Apps Script user-callback execution/authorization flow. Pairing is bound to the local Owner-approved installation, an unpredictable one-time claim, and a separate device polling proof; callback replay cannot change another installation. Google login is the only browser account action.

Create the owner's spreadsheet automatically, with three tabs: ประวัติการเข้าใช้งาน, เหตุการณ์ระบบ, สถานะระบบ. Initialize Thai headers and Asia/Bangkok display timezone; preserve UTC/time-quality fields. Persist the destination internally before returning success. Provisioning retries recover the tagged existing spreadsheet instead of intentionally creating another. Never delete unknown owner tabs/files. The device learns only safe account display, Sheet URL, link generation, and connection state through its authenticated polling request.

## 8. Device authentication and transport

Use an ESP-generated 256-bit installation-specific random ingestion secret, POST body only. Enrollment, polling, and ingestion proofs have separate purposes. No secrets in URLs, logs, Sheets cells, or browser persistent storage. Reconnect authenticates the existing installation and rotates the ingestion generation/key. Unauthorized/malformed/oversized requests fail closed.

Apps Script cannot read arbitrary incoming authorization headers: use a bounded first-line JSON envelope followed by the unchanged EventLog CSV bytes for uploads; JSON bodies for connection operations. Maximum CSV 8 KiB/32 events; whole envelope bounded separately. Apps Script ContentService redirects to Google content hosting: firmware may follow only a bounded HTTPS Google-content redirect, verify both certificates, never forward the POST body/proof on the redirected GET, and retain one total deadline. No general redirect support or insecure TLS.

## 9. Event IDs and deduplication

Reuse the existing CRC-validated EventLog schemas, stable event IDs and exact segment SHA-256. Store human columns plus installation ID, event ID, source digest and technical attribution in the same spreadsheet. Reject unknown secret-bearing fields. Write literal strings (RAW/stringValue) to prevent formulas. Preserve claimed versus verified identity and name/role snapshots; never invent identity or wall time.

Serialize receiver mutations with ScriptLock. A bounded per-installation pending-write reservation in Script Properties contains only segment digest, event IDs/digests and fixed target row addresses, not an event history database. Save that reservation before writing. Retry the same fixed ranges, never blindly append after an ambiguous write. Read back every addressed row before ACK. Existing event ID + same canonical event is already stored; conflicting content is rejected. A different batch cannot leapfrog an unresolved reservation. The spreadsheet is the sole remote history store. Manual deletion/corruption of technical columns must stop sync rather than produce false ACKs. Quota/size limits pause safely.

## 10–12. SD retry, confirmation, outage

Keep the existing worker scheduling, SD pinning, protected pending history, and exact seven-field segment receipt: protocol, COMMITTED state, installation_id, link_generation, segment_id, segment_sha256, ordered event_ids. COMMITTED means all events were verified in Google Sheets, including previously stored duplicates. HTTP 200 alone has no effect on SD. No valid complete matching receipt means retain pending data. Reboot/retry/lost ACK resend the identical immutable segment. Use bounded exponential backoff; never do TLS in the lock/touch main loop. Preserve existing bounded storage exhaustion/loss indicators and surface them in สถานะระบบ. Door authorization is entirely local during all cloud failures.

## 13. Disconnect and Factory Reset

Disconnect disables local uploads first and retries authenticated remote unlinking; revoke/remove that connection's server-side Google token without deleting its Sheet. Account permission revocation produces reconnect-required, not a local authorization change. Factory Reset clears local integration secrets/configuration as part of the existing reset policy, creates a new installation identity on fresh setup, and never deletes Google history. Offline reset cannot promise immediate remote token revocation; connection secrets are no longer usable by the reset device. Do not perform reset in this work.

## 14. Zero-cost verification

Apps Script consumer quotas and Sheets/Drive API free usage only. No Google billing account, paid quota increase, purchased domain/certificate, Cloudflare, D1, PostgreSQL hosting, or paid fallback. Exhaustion stops cloud work. The standard Google project is OAuth/API configuration, not a billable compute backend. Actual account/deployment/consent acceptance must be recorded separately from host tests.

## 15. Old architecture removal

Remove backend-worker and the obsolete Node/PostgreSQL service, deployment files and dependencies after inventory; retain reusable protocol/projection validation only in the Google receiver if needed. Replace the old manual Apps Script prototype. Mark former cloud designs/reports historical. Keep EventLog, local security, LockController and useful retry infrastructure. Verify remote resource identity before deleting anything; record absent resources as never created only with evidence. Revoke the experiment's Wrangler authorization after remote inventory. Leave unrelated services/resources untouched.

## 16. User actions

1. Open SmartLock Management.
2. Tap Connect Google Sheets.
3. Sign in to Google and grant permission.

## 17–18. Responsibilities and gates

SOL owns this design, all authorization/dedup/retention/transport decisions, each destructive cleanup approval, every major diff review and final PASS/FAIL. Luna High implements bounded receiver, firmware/UI adaptation, cleanup and focused tests only after assignment. Host mocks do not prove deployed OAuth, automatic Sheet creation, TLS behavior or the one-click flow. Those require G8 live Google acceptance. No COM6 flash in this phase.

## Current provisioning gate

The selected publisher account is blocked from creating the required Google project: the console reports mandatory two-step verification. A final reload still showed the block on 2026-09-27. No project, OAuth client, Apps Script deployment or spreadsheet was created; no trial/billing was enabled. This is an unresolved publisher prerequisite, not an additional owner setup step. G8 cannot pass until an eligible publisher deployment exists and the complete owner flow is exercised against Google. The firmware endpoint remains empty and synchronization is disabled by default.

## Primary references

- [Apps Script execution identities](https://developers.google.com/apps-script/guides/web)
- [Google-maintained Apps Script OAuth2 library and callback](https://github.com/googleworkspace/apps-script-oauth2)
- [Per-file Drive permission](https://developers.google.com/workspace/drive/api/guides/api-specific-auth)
- [Sheets creation accepts drive.file](https://developers.google.com/workspace/sheets/api/reference/rest/v4/spreadsheets/create)
- [Apps Script ContentService redirects](https://developers.google.com/apps-script/guides/content)
- [Apps Script quotas](https://developers.google.com/apps-script/guides/services/quotas)
- [Publisher verification requirements](https://developers.google.com/apps-script/guides/client-verification)
- [OAuth refresh-token lifetime and testing restrictions](https://developers.google.com/identity/protocols/oauth2#expiration)
