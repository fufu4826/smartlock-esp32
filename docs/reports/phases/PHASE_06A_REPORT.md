# Phase 6A — LAN / STA bootstrap and safe network handoff

Status: PASS for home STA association, LAN access, saved STA reconnect, and automatic Owner origin handoff. This is an intentional user-requested reordering of Phase 9 networking before remaining Phase 6 acceptance. Phase 6 implementation remains; substitute validation is paused and physical Phone #2/#3 validation is deferred.

## Architecture and Sol review

- Authenticated Owner/Admin Management provides scan, SSID selection, hidden password input, and connection testing. Credentials are not hardcoded, returned by status, or printed to Serial.
- WIFI_AP_STA keeps protected SmartLock-F0A4 at 192.168.4.1 active throughout the transition. Candidates remain in RAM until association and a nonzero LAN IP. Failure/20-second timeout retains AP and the previous confirmed configuration.
- Confirmed STA records use alternating CRC-checked NVS slots with readback. AP starts before saved STA on reboot and remains enabled after successful LAN use.
- The final handoff replaces window.opener/postMessage with an authenticated Owner-only random five-minute one-use bootstrap bound to the existing active Owner device and current LAN IP. Automatic LAN completion stores a new opaque browser credential locally and adds its salted verifier and ACTIVE Device associated with existing U000001. No duplicate Owner or repeated registration form is created.
- Failed or expired bootstrap cannot grant credentials or unlock. Former popup handoff routes are unregistered.

## Evidence

The user physically confirmed home Wi-Fi, AP-to-LAN transition, automatic handoff, Owner recognition, LAN Access QR, unlock, and timed relock. Earlier runtime checks observed saved STA reconnect at 192.168.1.179 with AP at 192.168.4.1. LAN health/status responded; unauthorized network APIs denied access. Final closeout evidence is recorded in PHASE_07_OWNER_ACCESS_REPORT.md.

Wrong-password transaction safety was reviewed; no new physical wrong-password test is claimed here. AP remains enabled permanently. DHCP address changes may require a fresh bootstrap for the new origin. Full Phase 9/recovery acceptance remains later work.

Physical Phone #2/#3 enrollment and revoke validation remains DEFERRED by user request on the Phase 14 checklist.

## Canonical clean acceptance supersedes legacy normal handoff

2026-09-26: User intentionally reset and repeated Setup using `http://smartlock-04225a0ff0a4.local` in one Chrome/profile. Normal AP-to-STA transition, unchanged browser credential, immediate Owner Access, physical unlock, timed relock, and consumed-URL replay rejection PASS. No handoff/second registration was used. Post-reset closeout build/COM6 flash PASS; boot SD database integrity PASS with exactly one User/one ACTIVE Device (U000001 Owner/D000001), saved STA reconnect, canonical mDNS and LOCKED start PASS. See CANONICAL_OWNER_ORIGIN_REPORT.md for full evidence, Windows OS resolver limitation, and deferred DHCP/multi-phone regressions. Historical descriptions of automatic normal-flow bootstrap above are superseded; bootstrap is explicit numeric-origin recovery only. Phase6A verified normal network transition PASS; Phase7 Owner Access PASS.
