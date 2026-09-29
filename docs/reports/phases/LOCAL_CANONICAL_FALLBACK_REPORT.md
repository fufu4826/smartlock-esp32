# Access and Management availability

Status: SOFTWARE PASS; REAL OWNER PHONE ACCEPTANCE PENDING.

Shared failure: canonical .local lookup failed before authentication, while LAN HTTP and direct mDNS queries succeeded. Exact phone/router multicast failure remains unproven; it is not an auth failure. One pre-change LAN sample timed out, followed by four successful health samples. No claim of permanent LAN stability from that sample.

Selected solution: existing protected AP, automatically retained while QR requested, canonical DNS/mDNS and a physical Wi-Fi join QR with Thai guidance. Sessions refreshed after joining; browser origin unchanged. Explicit mDNS address lifecycle/retry included. See design/LOCAL_CANONICAL_FALLBACK.md.

Validation: network 155 assertions/12 scenarios; canonical lifecycle 20 assertions; Admin 81 checks plus added fallback gesture/timeout/wrap checks; identity/Access/enrollment 349 assertions and20 fault boundaries; request policy43 checks and exclusive GPIO ownership; production Access/Management mobile Chromium same-origin continuity PASS. Desktop/host mocks do not prove actual Android DNS or magnet behavior. Layout preview is synthetic, not a device photo.

Build: 1238608 bytes, slot headroom 72112; static RAM113540. SOL reviewed integrated network/TFT diff: no auth bypass, no credential migration, no numeric QR URL, no LockController or LINE change. Approved normal application-only flash; runtime evidence to follow.

Acceptance remaining: Owner connects through TFT fallback if LAN .local fails, scans fresh Access QR, confirms actual page/auth/magnet release/LINE/relock; then Management reachable. No software evidence substitutes for these physical results.

## Deployment and final live evidence

Source commits: 2b4fcf2 (protected same-origin fallback), c229d41 (preserve healthy mDNS responder and disable modem sleep). Application-only COM6 flash at 0x10000 verified written hash. No partition/bootloader/NVS erase, Factory Reset or SD format.

The first deployment showed 0/20 LAN health replies at RSSI -90/-91 dBm and low contiguous heap/LINE ServiceError. Its evidence is preserved under attempt1-*; this was not accepted. The follow-up removes unnecessary mDNS task teardown and disables modem sleep. Initial follow-up probes passed 6/8; after settling and a normal reboot, 8/8 passed (31-235 ms), RSSI -69/-70 dBm. Radio conditions changed; these samples do not establish the cause of all earlier packet loss or permanent home-LAN multicast reliability.

Direct mDNS reply: canonical hostname resolves to 192.168.1.179. DNS port53 reply: protected AP address 192.168.4.1. Multicast query from PC still timed out. Thus mDNS responder is alive, while the exact phone/router multicast failure remains unresolved. Protected AP removes the home-router path; real Android fallback acceptance is PENDING.

Final clean-reboot diagnostics: GPIO22=1 LOCKED; configured/Owner/PIN/SD/calibration healthy; STA connected; temporary authorizations cleared; LINE configured v2 and Ready; free heap109148, minimum52040, largest55284. The earlier diagnostic involving a temporary full identity-array allocation also reduced contiguous heap, so its sample is not evidence of a persistent post-reboot LINE failure. No LINE engine changes or new test push were made.

Private NVS comparison: sl-config/sl-pin/sl-net/sl-sta/lock-touch-v2/sl-install/sl-line entries all identical. Raw private snapshots deleted after comparison; only booleans/counts retained. SD identity metadata still ko/D000001 OWNER ACTIVE and hi/D000002 USER ACTIVE, two identities/one Owner/two verifiers. Existing diagnostic OWNER_VERIFIER_PRESERVED=0 is the backup comparison flag, not proof of credential loss; valid Owner verifier exists. Browser continuity tested with production assets; actual Owner credential acceptance remains physical pending.

Inert historical SD files unchanged:23 files/18945 bytes/digest a63df454, new history writes DISABLED. LockController/authorization/LINE sources unchanged. No unlock was commanded by Codex.

SOL outcome: software/build/deployment and preservation checks PASS. Final availability acceptance NOT COMPLETE until Owner phone fallback Access and Management, actual magnet release/relock and LINE receipt are confirmed. No claim of permanent resolution yet.
