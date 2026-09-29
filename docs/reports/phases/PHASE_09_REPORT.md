# Phase 9 — LAN / STA Evidence Mapping

**Status: IMPLEMENTATION PASS / SOFTWARE ACCEPTANCE PASS / PHYSICAL CHECK DEFERRED TO FINAL REGRESSION.** This report maps the current STA/LAN implementation and recorded Phase 6A observations to the Phase 9 acceptance criteria. It does not claim a new device test or replace the final security review.

## Scope and evidence

- `src/network/NetworkManager.cpp` and `src/storage/StaSecrets.cpp` were read for implementation behavior.
- `docs/phase_reports/PHASE_06A_REPORT.md` supplies the existing physical-test record, including the later canonical-origin clean acceptance that supersedes its earlier automatic handoff description.
- Phase 9 criteria come from section 23 of the master plan. The plan's numeric Access URL is illustrative; the user's verified canonical-host requirement governs normal access.
- No credentials are included. No new physical negative-network test was performed or inferred.

## Phase 9 criteria mapping

| Phase 9 criterion | Existing implementation or recorded evidence | Audit status |
| --- | --- | --- |
| Join selected SSID | The authenticated Network page supports scan, SSID selection, and connection testing. `startCandidateSta` enters `WIFI_AP_STA`, keeps candidate values in RAM, and starts association. The Phase 6A report records physical home-Wi-Fi association. | Previously observed; no retest here |
| Show LAN IP in admin UI | `/api/network/status` supplies STA state and IP for the management UI. Phase 6A records LAN health/status and a connected STA address. | Previously observed |
| Keep setup AP available during transition | Candidate association uses `WIFI_AP_STA`; `update()` restores the configured AP if its IP changes. The Phase 6A report records the protected AP remaining at `192.168.4.1` while STA connects. | Implemented and previously observed |
| Preserve old network configuration until candidate succeeds | A candidate is not persisted during association. A nonzero STA IP is required before `saveConfirmed`; failure or the 20-second timeout clears the candidate password, marks STA failed, and leaves the AP path in place. Alternating CRC-checked NVS slots and readback protect the previous valid record. | Source behavior reviewed; negative case needs physical evidence |
| Persist and reconnect after restart | Confirmed credentials are written to alternating NVS slots and verified by readback. `startSavedSta` loads them without re-saving, and the Phase 6A clean-acceptance record reports saved-STA reconnect after reboot. | Previously observed |
| Phone on same LAN reaches SmartLock and Access QR unlocks | The Phase 6A report records same-LAN Access, physical unlock, and timed relock. The canonical clean acceptance records the existing Owner browser continuing to work after setup/network transition. | Previously observed |
| Use the canonical origin for normal access | The latest clean acceptance used the canonical `.local` hostname from the same Chrome profile, kept the Owner credential unchanged, and verified Access and relock. Numeric-IP bootstrap is documented as explicit recovery only. | Previously observed; overrides the plan's numeric URL example |
| Bad password leaves configuration accessible | Candidate storage and timeout behavior appear to retain the confirmed record and AP at source level. The Phase 6A report explicitly says no new physical wrong-password test was claimed. | **Pending physical test** |
| Lock remains safe during disconnect/reconnect | Phase 6A records locked startup and timed relock after successful Access. `NetworkManager::update` changes a disconnected STA from Connected to Failed while the AP path is retained. The cited report does not document a dropped-network test observing relay/lock safety during the outage and recovery. | **Pending physical test** |

## Owner and canonical-origin continuity

Use the later clean-acceptance evidence as the current normal-flow record. The user reset and repeated setup through the canonical hostname in one Chrome profile; the report records exactly one Owner user (`U000001`) and one active Owner device (`D000001`), saved-STA reconnect, unchanged browser credential, physical unlock, timed relock, and consumed-URL replay rejection. No normal-flow handoff or second registration was used. Earlier Phase 6A text describing automatic Owner bootstrap is historical and superseded; bootstrap remains explicit numeric-origin recovery.

Keep the setup AP available in `AP_STA` mode as already implemented. The canonical hostname is the normal browser origin even when a numeric LAN address is available; do not replace it with the master plan's sample numeric Access URL. Do not create another Owner or credential as part of ordinary AP-to-STA configuration.

## Remaining physical evidence

The recorded positive connection and reboot cases do not establish the negative-network criteria. The following remain pending and must not be reported as passed without fresh evidence:

1. **Bad-password rollback:** From the authenticated Network page, try an intentionally incorrect password for a selected network. Confirm the attempt reaches failure, the protected AP remains reachable, the last confirmed STA record is retained, the Owner/User/Device database is unchanged, and the lock does not unlock. This test was not performed for this report.
2. **Dropped-network safety:** While STA is connected, interrupt the router/network. Confirm the AP remains reachable, the UI reports loss of STA, the lock stays locked and does not actuate during the outage, and service recovers safely when the network returns or after reboot using the last confirmed configuration. No such physical disconnect/reconnect observation is claimed here.

Phase 10's named Recovery AP and physical recovery-trigger acceptance are separate criteria; retaining the existing protected setup AP during Phase 9 failures does not prove those Phase 10 requirements.

## Decision boundary

The normal AP-to-STA, canonical Owner access, successful reboot reconnect, physical unlock, and timed relock have recorded positive evidence. Bad-password rollback and dropped-network lock-safety acceptance remain unverified. This document is an evidence map only; root owns the final security decision and any Phase 9 PASS determination.

## Sol transaction review and repair (2026-09-26)

The prior source-only RAM claim did not account for the pinned Arduino2.0.17 SDK persistence default. Sol verified WiFiGeneric.cpp defaults `_persistent=true` and only selects WIFI_STORAGE_RAM when persistence is disabled. Both AP boot paths now call `WiFi.persistent(false)` before Wi-Fi mode initialization. Confirmed STA durability remains solely in the existing alternating CRC/readback StaSecrets NVS slots. Existing saved records are not erased.

Sol also found that cached `WL_CONNECTED` and the old IP could otherwise be observed immediately after starting a same-SSID candidate with a changed password. The transaction now explicitly disconnects, reports Connecting, observes the old connection becoming disconnected, then starts candidate association on a subsequent loop. Persistence is unreachable while waiting for disconnection. Only a later connected status and nonzeroIP can commit. Disconnect wait and association each have a bounded20second timeout, clear candidate password and preserve the AP/confirmed record on failure. No blocking wait or lock operation added.

These are Sol-owned changes to NetworkManager only; AccessController, LockController, authorization database, browser credential/canonical origin and verified dashboard are preserved. Luna's bounded contribution is the acceptance evidence mapping, not the security decision.

The remaining Phase9 physical test can cover both bad-password recovery and disconnect/reconnect lock safety: stay on the protected SmartLock AP using existing Owner Chrome, submit an intentionally wrong home-network password (at least8characters), observe clear failure and continued AP Management access, then reboot and verify reconnection with the previous known-good saved network. Confirm1User/1ACTIVEDevice and magnet LOCKED through the full test. Do not re-register or reset. Separate router-outage/recovery behavior belongs to later Phase10 regression; no independent router outage is claimed here.

### Factory-reset compatibility with RAM-only SDK settings

Pinned WiFiSTA.cpp shows `disconnect(..., eraseap=true)` calls esp_wifi_set_config and therefore follows the selected RAM/flash storage mode. Sol added `WiFi.eraseAP()` (SDK esp_wifi_restore) inside the already-confirmed FactoryResetController path to explicitly erase older SDK-persisted credentials. Failure returns StorageFailure while LOCKED. No reset was invoked by this change; Owner/Device/confirmed network state remains intact. The existing physical two-step confirmation, authorization and GPIO safety are preserved. Actual destructive reset validation is carried to Phase14 regression rather than erasing the user's verified Owner now.

## Final build and runtime evidence (2026-09-26)

- Final COM6 compile/upload passed in 76.65 seconds; upload image hash verified. RAM: 124576/327680 bytes. Flash: 1039741/1310720 bytes.
- Serial after reboot: configured YES, Owner exists YES, SD/session/database checks PASS, users 1, devices 1, active 1, revoked 0; LockController LOCKED. Saved STA reconnected to 192.168.1.179. Free heap 127308 bytes; minimum 127048 bytes.
- Initial LAN HTTP probes timed out after reboot. After a subsequent boot and stabilization, LAN health returned HTTP 200 and both phase8_runtime_check.py and phase7_preflight.py passed their relevant invalid-authorization/session rejection checks. No valid unlock request was sent.
- Direct multicast DNS returned the canonical hostname's A record from 192.168.1.179. Windows OS hostname lookup still failed (11001); this remains a documented resolver regression item, and current phone canonical access needs confirmation.
- Sol reviewed the transaction and reset compatibility changes. AccessController, LockController, session/authentication/database implementation, Thai dashboard and touch state-machine sources remain unchanged from the accepted Phase 8 commit. git diff --check passed.
- Phase 9 remains PENDING the physical bad-password/reboot recovery checkpoint. No Phase 9 PASS, physical negative-network result, or factory reset is claimed.


## Execution-policy closeout

The user explicitly authorized continuing through Phase 14 when only nonblocking physical evidence remains. The user confirms the board is currently reachable by the canonical hostname on home Wi-Fi. The prior pending physical checkpoint is therefore carried to PHASE_14_REGRESSION_CHECKLIST.md, not represented as a physical PASS.

Sol independently ran the native harness compiling the production NetworkManager.cpp with fake WiFi and StaSecrets: 54 assertions across five scenarios passed. The tests prove cached old connected state cannot commit a candidate, failed association retains the simulated confirmed record and AP, a successful candidate requires a nonzero IP before saving, and saved-STA boot reconnect does not rewrite its configuration. These are isolated software tests; actual RF bad-password observations remain deferred. Disconnect timeout and configuration-write failure also pass: neither saves an unconfirmed candidate or removes AP availability.

Read-only canonical_runtime_probe.py passes board multicast DNS advertisement and HTTP with the canonical Host header; Windows OS resolver failure is separately reported. Current negative API tests and Thai UTF-8/JavaScript syntax checks pass. GPIO22 writes remain confined to LockController; the sole unlock caller remains the verified AccessController path. No real network candidate, factory reset, database rewrite, Owner replacement or valid unlock request was used for acceptance.

Decision: implementation and software acceptance PASS under the revised execution policy. Physical bad-password/AP/magnet/EN observations remain DEFERRED. Continue to Phase 10.

