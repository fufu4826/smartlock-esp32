# SmartLock product scope

The production product provides Setup, Owner and authorized identities/devices, Access QR, Thai Management, physical Admin PIN, confirmed emergency timed unlock, deliberate Factory Reset, Wi-Fi/Recovery AP and local audit/event logging.

**Backup/Restore and browser OTA are intentionally removed.** Firmware maintenance uses USB/COM6. Their former HTTP endpoints, browser controls, package crypto, restore activation/journal engine and OTA transfer/boot-session machinery are absent. The existing application partition layout remains unchanged.

Core identity/auth atomic writes, verifier protection, Owner/role checks, session expiry/replay prevention, SD/database fail-closed validation and GPIO22 ownership remain required. Internal identity-migration preservation files are core integrity evidence, not a user Backup/Restore service, and remain intact. A tiny compatibility guard rejects a retired restore selector/journal instead of reviving stale legacy authority; it cannot activate or restore data.

Admin physical acceptance is already PASS. Cleanup does not overwrite the current PIN, identities, Wi-Fi or calibration and does not execute Factory Reset. Historical Phase 13 and Admin reports retain evidence for the earlier builds; they are not the current feature list. See PRODUCT_CLEANUP_REPORT.md in phase_reports for cleanup acceptance.

Google Sheets integration has been removed from the active product. Event history remains local on the device and is available through Management export. Earlier Google implementation reports and evidence are historical records only; see [the archive index](phase_reports/GOOGLE_SHEETS_ARCHIVE_INDEX.md).

Phone #3 and Windows .local remain deferred. Final from-zero acceptance is prepared only; no reset is authorized yet.

## Final fresh Setup

Owner name, unlock duration, explicitly chosen four-digit physical ADMIN PIN and confirmation only. Browser credential is generated automatically. No separate device/browser name or legacy Admin passphrase. Management uses the Owner browser credential, never the physical PIN. Fresh Setup rejects 1234. AuthStore schema 2 stores browser credential verifiers only; this fresh-state release does not migrate configured schema-1 databases. See phase_reports/SETUP_SIMPLIFICATION_REPORT.md.
