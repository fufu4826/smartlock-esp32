# Publisher setup progress — 2026-09-27

This is provisioning evidence, not end-to-end acceptance.

- Publisher account: <redacted-email>.
- Dedicated Google project: SmartLock Google Sheets / bright-petal-509909-c6.
- Google Auth configuration created; external audience remains Testing.
- OAuth web client created after explicit user approval. The client secret was transferred directly into receiver Script Properties and is excluded from source, reports, and terminal output.
- OAuth requested scopes: openid, email, drive.file. No broad Drive, Sheets, or cloud-platform scope is requested by the receiver.
- Publisher account added as the sole staging test user.
- Apps Script project: SmartLock Google Sheets Receiver, script ID 1-lAQ_PJN5zP80yEVJM3_jUW0wcOjxUby-oGim1jHrHuSvpdboaI7BoC8.
- Receiver source and manifest saved. User completed publication; Google displayed successful version 2 deployment.
- Public deployment ID: <redacted-apps-script-id>.
- Script Properties configured: SL:CONFIG:CLIENT_ID, SL:CONFIG:CLIENT_SECRET, SL:CONFIG:REDIRECT_URI. Values are deliberately omitted here.
- Anonymous malformed status POST returned HTTP 200 with protocol 1 / ERROR / invalid_status, through the expected script.googleusercontent.com content redirect. An earlier transient request returned 404; a subsequent controlled request succeeded. No successful event upload is claimed.
- Receiver host suite rerun: 19 tests passed, zero failures.
- No billing, free trial, payment method, COM6, production credentials, PIN, identities, Wi-Fi, or SD changes.

Still pending: real consent, automatic staging Sheet provisioning, exact upload ACK, duplicate/conflict checks, real literal-cell verification, and production audience readiness. Firmware endpoint remains unconfigured until acceptance.

## Real staging acceptance progress

- The user completed Google consent through the in-app browser.
- Authenticated pair_poll returned COMMITTED. Google created the private staging spreadsheet `SmartLock beb61204` automatically, with the three required Thai tabs and headers.
- Sheet: https://docs.google.com/spreadsheets/d/<redacted-sheet-id>/edit
- Live synthetic upload returned the exact seven-field COMMITTED receipt.
- Identical segment retry returned the exact ALREADY_STORED receipt.
- Same event ID with different canonical content returned CONFLICT.
- Browser inspection confirmed one synthetic access history row in the real Sheet. This is synthetic cloud evidence, not an actual door event.
- The first successful callback displayed failure despite the committed connection: completeProvision returns pairReply, but doGet expects result.ok. A focused correction and host regression are in progress.
- Google used a localized initial blank tab named Thai Sheet1. Existing code recognizes only the English default, leaving an extra blank tab. A locale-neutral new-sheet correction is in progress; existing Sheet data is preserved.
- This test used a local synthetic fixture, not the physical SmartLock Management flow. G8 product acceptance, production OAuth audience, board TLS/resource acceptance, and hardware deployment remain pending.

## Corrective receiver publication

- SOL reviewed both targeted corrections: successful callback requires COMMITTED before returning ok:true; localized initial-tab reuse is limited to newly created single-sheet metadata. Existing multiple-sheet data is preserved.
- Receiver host suite: 23 tests passed, zero failures, including production doGet success/denial/exchange-failure and localized/existing-sheet cases.
- Google confirmed version 3 publication at the same approved deployment URL, with unchanged publisher and public access permissions.
- The corrected first-callback page and locale-neutral fresh creation are host-verified and deployed, but have not yet been exercised by a second live consent. The existing staging Sheet retains its extra initial blank tab; no user data was deleted.
- Full release remains pending: production audience/branding, complete physical Management connection acceptance, board TLS/resources. No COM6 or commit.
