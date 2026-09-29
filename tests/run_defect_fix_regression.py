"""Focused SL-03..09 integration. Host only; never opens COM6 or a real browser."""
from pathlib import Path
import json
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs/phase_reports/evidence/defect_fix_03_09"
COMMANDS = [
    [sys.executable, "-B", "tests/registration/run_registration_tests.py"],
    ["node", "tests/registration/registration_browser_test.js"],
    ["node", "tests/registration/session_display_test.js"],
    [sys.executable, "-B", "tests/identity/run_identity_tests.py", "--session-display"],
    [sys.executable, "-B", "tests/identity/run_identity_tests.py", "--management-expiry"],
    [sys.executable, "-B", "tests/http_deadline/run_http_deadline_tests.py"],
    [sys.executable, "-B", "tests/identity/run_identity_tests.py"],
    [sys.executable, "-B", "tests/identity/run_identity_tests.py", "--cleanup-smoke"],
    [sys.executable, "-B", "tests/admin/run_admin_tests.py"],
    ["node", "tests/admin/web_test.js"],
    [sys.executable, "-B", "tests/network/run_network_manager_tests.py"],
    [sys.executable, "-B", "tests/security/run_security_policy_tests.py"],
]

if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    results = []
    with (OUT / "host-regression.txt").open("w", encoding="utf-8") as log:
        for command in COMMANDS:
            started = time.monotonic()
            result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                                    encoding="utf-8", errors="replace")
            entry = {"command": command, "exit": result.returncode,
                     "seconds": round(time.monotonic() - started, 2)}
            results.append(entry)
            log.write("\nCOMMAND: " + " ".join(command) + "\n")
            log.write(result.stdout + result.stderr)
            log.write("\nEXIT: " + str(result.returncode) + "\n")
            log.flush()
            print(json.dumps(entry), flush=True)
            if result.returncode:
                print(result.stdout + result.stderr)
                break
    (OUT / "host-regression.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    sys.exit(0 if len(results) == len(COMMANDS) and all(x["exit"] == 0 for x in results) else 1)
