# Management display/lock-section removal

Only production change: src/web/WebAssets.h. Removed display and lock-settings navigation anchors, placeholder sections and JavaScript page-map entries. No dedicated endpoints or edit handlers existed for these placeholders; therefore no API removal was necessary. Unknown old page hashes fall back to Dashboard. Shared summary, Network and System/Admin controls remain.

Final sections: Dashboard, Authorized devices, Network, Logs, Cloud (existing, unchanged and deployment paused), System. No Backup/Restore/OTA controls. Thai UI retained.

Setup and stored unlock duration unchanged. Access and physical Emergency paths both use config.unlockDurationMs; LockController polarity/ownership/timer unchanged. DisplayManager, TouchManager and AppStateMachine source unchanged. No identity, PIN, network or event-storage code changed.

Preflight found Setup had already completed: one ACTIVE OWNER ko/D000001 and one verifier. Preserve this configured state; no reset or registration performed by Codex.

Focused checks PASS: Management DOM/dead-control/navigation test, Setup browser fields/duration, Admin web controls, 78 Admin/PIN/config/lock checks and 10 router/gesture cases, core Access/Management/storage smoke, GPIO source policy. These are host checks, not a new physical Access/wake/sleep acceptance. Full production build invoked once. Deployment/evidence follows.

Build/flash PASS. Before 1,239,632 bytes; after 1,238,752 bytes; recovered 880 bytes. Static RAM 108,468 bytes. One build only, then direct COM6 application upload with hash verification (no partition/NVS write). Manifest in evidence/management_trim/build.json.

Normal boot PASS: GPIO22/LockController LOCKED; Configured YES; one ACTIVE OWNER ko/D000001 and one verifier; SD/calibration/PIN store healthy; STA reconnects to 192.168.1.179; canonical mDNS OK; no panic/watchdog. No Owner/config data changed. OWNER_VERIFIER_PRESERVED=0 refers to absent old migration backup on fresh schema-2 Setup, not failed current credential validation. Live /manage returned the new page with both sections absent and PIN/Network controls present. Physical authenticated Access/relock and TFT wake/sleep were not newly exercised in this cleanup; shared implementation remained unchanged and focused host regressions passed. No reset, registration, cloud deployment or unlock initiated.
