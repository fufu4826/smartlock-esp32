HISTORICAL: Google Sheets integration was removed from the active product. This document records earlier diagnostics and is not current product acceptance.

# H1 canonical hostname diagnosis

Status: board mDNS responder and HTTP service advertisement VERIFIED; phone resolution retest pending. Google connection is paused at the user's request. No firmware edits, reflash, Wi-Fi/router changes, reset, credential changes, or SD/NVS erasure are part of this diagnosis.

The Owner phone reported Chrome `DNS_PROBE_FINISHED_NXDOMAIN` for `http://smartlock-04225a0ff0a4.local` before Management loaded. The user confirmed home Wi-Fi; the exact phone subnet, access point and guest/VLAN isolation are not yet verified.

## Board and HTTP evidence

- Fresh monitored H1 boot: STA IPv4 `192.168.1.179`, canonical hostname `smartlock-04225a0ff0a4.local`, mDNS initialization OK, HTTP initialization OK.
- Subsequent read-only serial diagnostics: STA connected, GPIO22 HIGH/locked, configuration/Owner/SD/calibration/PIN store healthy. No observed panic/watchdog. RSSI is not exposed by the existing safe serial command; no value is invented.
- PC: `192.168.1.145/24`, gateway `192.168.1.1`, Wi-Fi interface on 5 GHz. Gateway ping succeeded. Board ARP initially failed, later resolved to `A4-F0-0F-5A-22-04`.
- Five-minute IP health sampler: 30 samples, 11 successful exact HTTP 200 `ok` responses, 19 timeouts. Recovery occurred at sample18; samples29-30 timed out again. This is intermittent LAN reachability, not a clean HTTP soak PASS.
- Post-recovery root and health requests succeeded, retired endpoints returned410, and unauthenticated cloud status returned403. Management HTML retrieval timed out. These requests did not create credentials or log in at the numeric origin.

## mDNS source review

`src/network/CanonicalOrigin.cpp` builds label `smartlock-04225a0ff0a4` without `.local`, passes that label to `MDNS.begin`, and advertises `_http._tcp` port80. The `.local` suffix is used only for the canonical FQDN/URLs.

`src/main.cpp` starts mDNS on the configured AP before starting saved STA. Installed framework ESP-IDF is4.4.7. Its `mdns_init` registers Wi-Fi and IP handlers; STA disconnect disables the STA responder and STA GOT_IP enables it. Thus initialization before obtaining STA IP is supported and does not itself prove a defect. See the [version-matched Espressif source](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.7/components/mdns/mdns.c), `_mdns_handle_system_event` and `mdns_init`. No deliberate reconnect or reboot was induced for this diagnosis.

## Live name-resolution evidence

- Direct UDP mDNS A query to board port5353 returned a62-byte reply from `192.168.1.179:5353`, with one answer containing the exact canonical label and STA address. The responder is running; a missing/misspelled hostname is not established.
- Initial multicast A queries, including an explicitly selected Wi-Fi interface with multicast TTL255, received no answer within4-5seconds.
- Final correctly joined port5353 queries used transaction ID0 and four-second receive windows. A multicast QU query for the canonical HTTP service returned SRV `smartlock-04225a0ff0a4.local:80` and A `192.168.1.179` from the board. Direct QU A/PTR/SRV queries also answered with the correct hostname and port. Evidence: `evidence/google_h1/mdns-wire-txid0.txt`.
- Normal QM multicast answers were not observed on this Windows socket. QU multicast reception proves at least one multicast query reached the board; it does not prove the phone receives multicast answers or that the router filters them.
- Windows `getaddrinfo` failed with11001; in-app browser navigation failed `ERR_NAME_NOT_RESOLVED`.
- Windows Wi-Fi profile is Public with firewall enabled and multiple existing UDP5353 consumers. Missing multicast replies alone cannot distinguish LAN filtering, Windows receive filtering, interface behavior, or a responder multicast fault.

Current conclusion: board-side responder and service advertisement PASS, with multicast delivery/client resolution and intermittent LAN reliability still unresolved. No proven firmware defect justifies a reflash. Canonical hostname is unchanged. Do not blame Android or require permanent phone DNS changes without further evidence. A diagnostic phone visit to `http://192.168.1.179/health` has been requested; it creates no credentials and is not a replacement product origin.
