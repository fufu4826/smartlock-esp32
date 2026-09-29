# LINE web administration and delivery diagnosis

Status: software regression and deployment PASS; human notification receipts pending.

## Diagnosis

The installed firmware retained four queued events with zero successful sends. Its largest free block was 34,804 bytes, below the unchanged 40,000-byte TLS admission guard. This is a real delivery failure, not evidence of delivery. The pre-870ec7f evidence also reports transport state 5. The 870ec7f diff changed navigation and TFT content, not transport or event hooks; that commit alone is not proven to have introduced the fault.

Completed HTTP requests retained allocation capacity despite argument-content wiping. The server now releases completed request arguments, header values and response buffers after handler completion. Unnecessary mDNS TXT mutation on interface changes was removed; IDF retains interface-event responsibility. Live validation must determine whether these allocation changes resolve the blockage. The TLS threshold has not been lowered.

Safe pipeline counters expose generation, enqueue, admission heap, TLS connection, write, response and retirement status without secrets.

## Administration

LINE maintenance is Owner Management plus the existing AdminPin verifier. Incorrect completed comparisons invoke the same failure callback, durable counter and lockout as physical Admin. A random one-use nonce is bound to the Owner token and PIN revision, expires after 120 seconds, requires LOCKED state and clears on success, cancellation, disconnect, replacement login or PIN change. Commit writes only LINE configuration. No token or recipient input is rendered in Management; the authenticated developer POST is separate from normal UI.

Physical LINE setup, its Serial frame parser and USB helper are removed. Existing Emergency Unlock, Factory Reset authorization, PIN and GPIO ownership remain. Existing PIN state is never overwritten. Only genuinely new unconfigured installations receive initial 1234; Setup can replace it with the chosen PIN.

## Focused evidence

- Production AdminPin host tests: 87 checks; physical Admin/router/gesture regression: 10 cases.
- Identity/auth regression: 349 assertions and 20 migration fault boundaries.
- Security policy: 43 checks and GPIO ownership verification.
- Network: 155 checks across 12 scenarios; canonical lifecycle: 20 checks.
- HTTP total deadline: 11 checks; touch and LINE queue/parser/event regressions PASS.
- Source-extracted LINE maintenance handlers PASS. Authorization, PIN and storage dependencies are fakes in that harness; actual AdminPin and identity behavior are tested separately.
- Production JavaScript setup/admin tests PASS; mobile production Management HTML with mocked endpoints PASS.

## SOL review

Integrated source review approved scoped web authorization, PIN reuse, nonce retirement, USB removal, secret exclusion, lock ownership and retained SD authorization. Network notification work stays after local lock operations. No persistent event history is restored. No Google integration is added.

## Resources

Deployed firmware binary: 1,238,000 bytes; application slot: 1,310,720 bytes; headroom: 72,720 bytes; static RAM: 112,612 bytes. Implementation commit: 979fc74405944e28375d92bdcb870b5aed1c1f6a.

## Live acceptance

Application-only COM6 deployment verified written binary hash. NVS namespace comparison reports identical sl-config, sl-pin, sl-net, sl-sta, lock-touch-v2, sl-install and sl-line values; private temporary flash snapshots were deleted after comparison. Read-only authentication inspection confirms ko/D000001 Owner Active and hi/D000002 User Active, two identities, one Owner and two verifiers. No PIN change, reset or storage formatting was performed.

After verified TLS quota requests, LINE state is Ready, TLS connections/responses are two, heap-blocked count is zero. Largest block remains 55,284 bytes after a second normal boot (57,332 on first boot), free heap approximately 109 KB, minimum heap 51,692. Original 90 KB free / 40 KB largest-block admission limits remain unchanged. The memory blockage is resolved in these live checks; long-running transport and event receipts still require the three real event tests.

Live Management is served without raw credential fields. Twelve health requests passed; unauthorized arm/commit returned 403; old save route returned 404. Inert SD history remains 23 files, 18,945 bytes, digest a63df454, writes DISABLED. Boot and final diagnostics report LOCKED/GPIO22 HIGH, Wi-Fi connected, PIN store healthy, calibration loaded and unchanged installation. Weak RSSI around -90 dBm on the final boot kept Recovery AP enabled; this is observed network policy, not a notification authentication failure.

In-app canonical navigation timed out on the development browser; direct diagnostic IP HTTP works. No browser identity was migrated or bypassed. Registered Owner interaction is required to verify the protected web PIN workflow. Pending: one test push and human receipt for normal Owner unlock, first incorrect PIN and Emergency Unlock. No successful push or human receipt is claimed for this deployment yet. PIN-change/persistence behavior passed production-method host tests; the installed chosen PIN remains unchanged.
