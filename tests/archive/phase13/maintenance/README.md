# RestoreCoordinator host fault tests

Run with `python tests/maintenance/run_restore_coordinator_tests.py`.

The test compiles the actual portable `src/maintenance/RestoreCoordinator.h` kernel. Its durable fake models every generation as one indivisible SD identity/verifier snapshot plus matching NVS config/network bundle and an atomic active selector. It injects before/after interruption points in candidate SD write, SD rename, NVS staging, each activation journal boundary, selector switch, and finalization.

The host fake is not the production SD/NVS backend. These tests prove only the coordinator template's decisions under the fake's durability contract. They do not prove actual card rename durability, NVS commit atomicity, board binding, encrypted backup parsing, reboot GPIO state, or live restore acceptance.

## GenerationStore integration host tests

Run with `python tests/maintenance/run_generation_store_tests.py`. This compiles the actual `src/storage/GenerationStore.cpp`, `AtomicFileStore.cpp`, `ConfigStore.cpp`, and `RecordCodec.cpp`. It exercises activation, reboot recovery, initialization gating/finalization, updates after finalization, invalid candidate rejection, NVS before/after-commit faults, every fake SD mutation failure boundary, and corrupted selector/journal records.

The production `SecureBackup` implementation is replaced by a focused host fake. The fake checks snapshot framing, board value, config validation, and snapshot CRC; capture and owner equality use fixed test identity/verifier content. It does not test encryption, passphrase handling, actual identity/authentication parsers, flash or card power-loss behavior, Preferences atomicity, physical GPIO/door control, or live restore. NVS records persist byte-for-byte across simulated `GenerationStore::begin()` calls, with selectable failed/torn puts; SD uses the existing in-memory `tests/identity/mocks/SD.h`, whose mutation injection fails before the selected mutation. These tests verify the concrete production method bodies only under those fake storage contracts.


## Snapshot and OTA validation suites

- `python tests/maintenance/run_snapshot_validator_tests.py`: production SnapshotValidator/AuthSnapshot plus production configuration, name, record and STA validation; 20 semantic snapshot assertions. No encryption or physical storage is tested here.
- `python tests/maintenance/run_ota_tests.py`: production OtaTransfer; 113 bounded streaming/header/order/timeout/updater-failure checks with a fake updater.
- `python tests/maintenance/run_ota_manager_tests.py`: production OtaManager method bodies, dependency include substitution only. 186 checks across 20 subprocess executions, including persisted receipts surviving process restart. SDK, NVS, authority, capture and hash are fake; the deterministic test hash is NOT cryptographic. Tests cover authorization expiration/revocation, inactive updater failures, receipt-before-boot order, invalid capabilities, preserved snapshot/image comparison, STA startup gate and offline timeout. These do not establish real flash/NVS durability, actual SHA correctness, HTTP transport or live OTA partition switching.

No suite changes the production board or contains a production browser credential.
