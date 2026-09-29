HISTORICAL: Google Sheets integration was removed from the active product. This document records earlier diagnostics and is not current product acceptance.

# Google Sheets hardware deployment phase H1

**OVERALL: H1 PARTIAL.** The reviewed Google-enabled application was flashed and persistent state checks passed. Google connection is paused at the user's request after the Owner phone reported canonical-host NXDOMAIN. LAN reliability, real-board cloud sync/TLS, and physical acceptance remain open. See [canonical-host diagnosis](H1_CANONICAL_HOST_DIAGNOSIS.md).

## Release and build

The reviewed release is commit `c6c90af967d4bf361047b2e4bb435a0ac132fb37` (`Release one-click Google Sheets integration with verified receipts`). SOL reviewed the pairing and Management authorization gates, TLS host/certificate and redirect handling, exact receiver acknowledgements, and the requirement to revalidate the local segment before retiring SD data. Source review also confirmed that Google paths do not control the lock output and `LockController` owns GPIO22. These are source-level findings; they do not establish live behavior on the board.

`pio run -e esp32_035` completed successfully with the accepted publisher version 4 endpoint configured. The linker reports 1,250,553 bytes of application use in the 1,310,720-byte app slot, leaving 60,167 bytes by that measure. The produced `firmware.bin` is 1,257,136 bytes, leaving 53,584 bytes when compared directly with the app-slot size. Static RAM use is 118,276 bytes of 327,680. The binary SHA-256 is `f880bd22a293abbd550a34dd2122d785ee2537da501ff55653e5cde8aeda4860`.

The generated partition table has app0 at `0x010000` and app1 at `0x150000`, each 1,310,720 bytes. A fresh live partition read matched the built table exactly, with erased padding, and OTA selection matched app0. SOL approved an application-only esptool write at `0x10000`; 1,257,136 bytes were written and the tool reported `Hash of data verified`. No bootloader, partition table, OTA-data, NVS, or SD image was written. The upload was followed by normal boot and a later monitored normal reboot for persistence verification. Factory Reset was NOT PERFORMED.

Build details and hashes are in [H1 build evidence](evidence/google_h1/build-summary.json) and [the build log](evidence/google_h1/build.txt).

## Host and browser evidence

Seven focused suites passed with 278 assertions or cases total: receiver core (23), publisher pages (2), protocol (63), transport (55), sync and pair parser (44), HTTP routes (43), and browser VM (48). The offline Management browser fixture also passed: it exercised the production page against a local consent stub, kept pairing pending through a simulated transport failure, then recovered to connected. It made no Apps Script call and used no board. Results are in [H1 host test evidence](evidence/google_h1/host-tests.txt).

Separately, [final Google acceptance](GOOGLE_SHEETS_FINAL_GOOGLE_ACCEPTANCE_REPORT.md) records live Google consent, spreadsheet creation, exact upload acknowledgement, duplicate retry, and controlled destination failure/recovery using disposable staging data. That evidence validates the receiver and owner flow across a host device boundary; it is not ESP32 TLS or hardware acceptance.

## Read-only COM6 baseline

The pre-flash snapshot was collected from COM6 at 115200 baud with DTR and RTS disabled before opening the serial port. Only source-reviewed `DIAG_AUTH` and `DIAG_SYSTEM` diagnostics were sent. No reset, reboot, flash, configuration write, PIN attempt, unlock, HTTP mutation, or audit-dump command was issued.

The active database reported two identities, one Owner, and two active verifiers: Owner `D000001` and User `D000002`. The device reported configured with an Owner, a 5,000 ms unlock setting, healthy SD and calibration, and GPIO22 high/locked. The installation and network were healthy. The existing firmware reported Google disabled and unconfigured, with 103 events in 19 pending segments and zero history-loss or unrecorded-event counters. These are the pre-flash values only; they do not describe the future firmware run. PIN and verifier material, Wi-Fi credentials, account details, spreadsheet titles, and URLs are omitted from the evidence.

The full snapshot is [the H1 pre-flash evidence](evidence/google_h1/pre-flash.txt). This baseline supports preserving the existing NVS, SD history, identity records, and network configuration. It does not establish behavior after the new app image starts.

## Post-flash state and boot: PASS

The monitored boot reported LockController LOCKED, GPIO22 LOCKED, hardware lock timer OK, SD OK, touch calibration loaded, identity database PASS, HTTP OK and canonical mDNS initialization OK. STA connected at `192.168.1.179`. Owner `ko/D000001` and User `hi/D000002` remain ACTIVE, with one Owner and two verifiers. Installation `3676c44dffc173298feb01620c8d00cf` and unit `000004225a0ff0a4` are unchanged. Unlock duration remains5seconds; temporary Access/Management/Enrollment/Admin authority is absent after boot.

Private before/after NVS comparison confirmed byte-identical logical values in `sl-config`, `sl-pin`, `sl-net`, `sl-sta`, and `lock-touch-v2`. Only booleans/counts were recorded; temporary private snapshots were removed. This verifies chosen PIN storage, network/configuration and calibration preservation without disclosing secrets. Current credential usability still needs the normal Owner browser test.

Pending history progressed103events/19segments →106/20 after deployment →109/21 after the monitored reboot, with zero reported history loss/unrecorded events and zero cloud uploads. The increase is consistent with local boot/network events; no history was manually removed. Google reports ready=true, enabled=false, configured=false. The boot banner still says cloud paused; runtime ready/enabled state is the authoritative diagnostic.

The legacy `OWNER_VERIFIER_PRESERVED=0` flag compares the current Owner verifier with an old migration backup. It was0 before and after deployment, while the current database is valid. No migration/archive repair was performed.

## Resource and LAN evidence: acceptance incomplete

Serial monitoring recorded17 complete samples over264seconds, then an incomplete response; a separate bounded30second recheck completed normally. This is not a continuous five-minute capture. No panic, watchdog or unexpected reboot was observed. The final recheck reported128,788bytes free heap,103,620minimum,73,716largest block. A sampled largest-block trough was59,380bytes. These are idle/local-HTTP measurements; repeated real ESP32 TLS transactions have NOT been exercised.

The five-minute LAN sampler returned11 successful exact health responses and19timeouts from30samples. Initial requests failed; samples18-28 succeeded; samples29-30 timed out. A post-recovery check reached root/health, retired routes410 and forbidden cloud status403, but Management HTML timed out. Thus reliable LAN/Management responsiveness and active-cloud soak are NOT PASS. No router, workstation network or board Wi-Fi setting was changed.

## Remaining gates

The Owner phone reported `DNS_PROBE_FINISHED_NXDOMAIN` at the canonical origin before Management. Google connection is explicitly paused until that canonical page opens. Numeric IP is diagnostic only; no credentials were created there.

Real-board Google pairing, TLS, existing-history upload, exact ACK followed by actual SD retirement, interruption/retry, Google row verification, and active-cloud soak remain NOT TESTED. The receiver's earlier staging acceptance and host tests do not replace these hardware gates.

Normal Owner Access software acceptance is pending. Physical magnet release/relock, touch responsiveness, and current/old PIN checks require user observation; no physical PASS is claimed. Source/serial evidence does not prove absence of a brief electrical pulse during reset.

Evidence: `evidence/google_h1/` contains release review, pre/post state, build, flash, sanitized NVS comparison, LAN and serial monitoring. The requested hardware phase cannot be declared complete until canonical access and the remaining live/physical gates pass. No Factory Reset, re-enrollment, credential replacement, broad erase, or second firmware upload occurred.
