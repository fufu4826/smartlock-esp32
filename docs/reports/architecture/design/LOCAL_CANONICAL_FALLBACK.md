# Same-origin local fallback

Decision: keep home-LAN canonical access and automatically offer the existing protected Recovery AP while Access or Management QR is shown. No numeric-IP credential migration and no external service.

A (same canonical hostname on protected AP) is selected. B (AP-only entry) would unnecessarily replace convenient LAN use. C (new IP origin plus credential migration) adds authentication complexity and cannot access the existing canonical browser credential when DNS fails. D (new local hostname/native app/router DNS) either changes origin or adds setup/software. None is required for this device.

The board cannot detect a phone DNS failure before receiving a request, and a web page that cannot load cannot redirect itself. Therefore the AP starts with either QR. If the page fails, the visible Thai TFT button opens a standard Wi-Fi join QR. The phone connects, stays on this network even without Internet, then the screen returns to a fresh canonical Access/Management QR. The original sessions are invalidated while the Wi-Fi QR is shown. No IP entry, Owner recreation or credential copying.

The AP uses the existing WPA key; only the physical Wi-Fi join QR contains it, never HTTP/Serial/Git. Joining grants network connectivity only. Existing Owner credential verification is still required; Recovery Access/Management remain Owner-only and enrollment remains LAN-only. AP clients prevent AP closure; otherwise a five-minute lease pins fallback against LAN probes. After expiry existing LAN shutdown policy resumes. STA/LINE stay connected.

Origin continuity: same scheme, hostname and port regardless of transport. Production assets in mobile Chromium used one unchanged Owner fixture across two local receivers; this is browser proof, not phone DNS or magnet proof.

Android chooses mDNS for .local on supported resolver versions, so wildcard AP DNS alone is not a universal solution. The direct AP also advertises mDNS without the home router; actual Owner phone acceptance remains mandatory. Source: https://source.android.google.cn/docs/core/ota/modular-system/dns-resolver?hl=en (checked 2026-09-28). Existing IDF interface events remain supported; settled IP/interface changes refresh the existing service record without restarting a healthy responder; failed starts/refreshes retry at bounded intervals. Modem sleep is disabled using the framework Wi-Fi API, improving local responsiveness without changing credentials. Espressif documents power-saving multicast limitations: https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-guides/wifi.html (checked 2026-09-28).

LINE implementation, tokens and queue are unchanged. LockController, GPIO22 polarity, authorization, PIN, SD identity DB and event-history disabled state are unchanged.
