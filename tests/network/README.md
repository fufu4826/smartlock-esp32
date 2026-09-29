# Isolated NetworkManager host tests

These tests compile the production `src/network/NetworkManager.cpp` on the host. Arduino, WiFi, canonical-origin, and `StaSecrets` calls are supplied by test fakes. The `StaSecrets` fake stores credentials only in test-process memory; no real radio, ESP32 NVS, database, or hardware is accessed. Test output contains scenario names and assertion counts only, never credential values.

From the repository root, run:

```powershell
python tests/network/run_network_manager_tests.py
```

The runner uses `CXX` when set, then `g++`, `clang++`, or a Visual Studio 2022/Build Tools installation found by `vswhere`. It builds into a temporary directory and removes that build output when the run ends.

Covered behavior:

- A stale `WL_CONNECTED` indication for an old connection cannot start or persist the new candidate before disconnect is observed.
- A candidate is saved once only after `WL_CONNECTED` and a nonzero local IP; timeout failure preserves the previous saved configuration and configured AP.
- A saved boot configuration can connect without being written again.
- A lost STA connection retains the normal AP for 60 seconds, then activates `SmartLock-Recovery-...` with the configured AP password and retries only stored, known-good STA credentials without rewriting them.
- When no confirmed STA configuration exists, recovery still activates the protected AP and performs no blank retry or configuration write.

`NetworkManager.cpp` is compiled without application database modules, and its mocked recovery scenarios make no database calls. The tests therefore verify that this networking recovery path has no database dependency or mutation path; database contents themselves are outside this unit's scope.
