# Google Sheets archive index

HISTORICAL: Google Sheets integration was removed from the active product after the checkpoint commit and tag recorded in [GOOGLE_SHEETS_ARCHIVE_CHECKPOINT.md](GOOGLE_SHEETS_ARCHIVE_CHECKPOINT.md). These documents and evidence describe earlier code, receiver deployments, and test runs; they do not describe current firmware or current product behavior.

Historical design documents are retained under `docs/design/GOOGLE_SHEETS_*.md`. Historical implementation and acceptance reports are retained under `docs/phase_reports/GOOGLE_*.md`, excluding this index, the archive checkpoint, and any later removal report, plus `docs/phase_reports/PHASE_12_REPORT.md`. The current GOOGLE_SHEETS_REMOVAL_REPORT.md is the removal decision and is not an archived implementation plan. Their evidence remains under `docs/phase_reports/evidence/google_*`. Do not interpret a historical PASS, build, upload, or consent receipt as current product acceptance.

The active product keeps local EventLog storage, history retention, and Management CSV export. Cleanup removes only Google transport, receiver, pairing, sync, and their focused test infrastructure. It does not delete remote receiver deployments or touch device-resident local history.
