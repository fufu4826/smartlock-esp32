# SmartLock passive LINE heap baseline — 2026-09-28

Scope: read-only characterization on the previously identified SmartLock at COM6. No reset, PIN, unlock, LINE test/event, enrollment, configuration write, source change, build, or flash was performed during this capture. COM6 remained the sole present USB-SERIAL CH340 adapter (VID:PID 1A86:7523, location 1-2), matching the saved C5/C6 deployment identity record. Only fixed `DIAG_SYSTEM` commands were sent with DTR and RTS false before opening the serial port. Raw serial lines, station IP, installation identifiers, and page response bodies were not saved or printed.

## Idle series before HTTP requests

Seven complete samples were captured from 2026-09-28 17:03:17 to 17:06:21 +07:00 (184 seconds). STA remained connected; AP remained enabled with zero AP clients. Free heap was 109,336 bytes in every sample; minimum free heap was 55,268 bytes; largest free block was 34,804 bytes. LINE queue, event generation, enqueue, push-start/write, TLS connections, and HTTP responses were unchanged (all event/push counters zero; TLS connected 2; responses 2). `heap_blocked` increased from 16,818 to 17,002 (+184 over 184 seconds). The counter is reported without causal attribution.

## Canonical `/health` request and AP transition

A first attempt to resolve the saved canonical `.local` hostname failed locally (`gaierror`) and did not reach the device. The actual request used the privately read RFC1918 STA address with the saved canonical hostname in the HTTP `Host` header; the address was withheld. One unauthenticated `GET /health` returned HTTP 200 at 17:08:30–17:08:34 +07:00, with no response body retained. AP was enabled with zero clients immediately before the request (17:08:29) and disabled 3.25 seconds after request completion (sample at 17:08:38); STA and HTTP remained ready. Free heap changed from 109,336 to 111,888 bytes; minimum stayed 55,268; largest stayed 34,804. `heap_blocked` changed from 17,130 to 17,138 (+8 over approximately 9 seconds). LINE event/push counters remained zero, TLS/response totals stayed 2/2, and the lock/GPIO22 diagnostic stayed locked.

## Inert page GETs and post-request idle series

One `GET /manage` and one `GET /a/<dummy 64-zero route>` each returned HTTP 200. The Access request used a deliberately invalid dummy route token; no registered Owner credential or real QR/session token was supplied. Both response bodies were discarded and no browser JavaScript executed. Neither request changed the observed AP state (off before and after).

Seven post-request diagnostics from 17:10:17 to 17:13:21 +07:00 (184 seconds) showed AP off, zero clients, and STA connected throughout. Largest block remained 34,804 bytes and minimum free heap remained 55,268 bytes. Free heap ranged from 111,440 to 112,144 bytes. `heap_blocked` rose from 17,238 to 17,421 (+183 over 184 seconds); TLS/responses remained 2/2; LINE queue, generated, enqueued and push counters remained zero; locked/GPIO22 remained locked.

## Browser matrix and limitations

No SmartLock tab was present in the available browser inventory, so registered-browser Management and Access page loads are unobserved. The direct HTTP page GETs above exercised only the static unauthenticated GET routes and do not prove browser behavior or phone receipt. No LINE send or receipt was tested. The heap counter continued to rise at approximately the same observed rate before and after the AP transition; this capture alone does not identify its origin. The measurements are bounded point-in-time diagnostics, not heap-task traces or root-cause attribution.

Evidence files: `idle_samples.json`, `health_get_ap_transition.json`, `health_get_ap_transition_direct_sta.json`, and `page_gets_and_post_idle_samples.json`.
