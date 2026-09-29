from datetime import datetime, timezone
from pathlib import Path
import re
import sys
import time

import serial


OUT = Path(__file__).resolve().parent / "production_restore_live_capture.txt"
PORT = "COM6"
BAUD = 115200
SAFE_BOOT_EXACT = {
    "BOOT",
    "LockController: LOCKED",
    "GPIO22: LOCKED",
    "LOCK TIMER: OK",
    "Touch calibration loaded",
    "ADMIN PIN STORE: OK",
    "LINE: READY (Owner Management + ADMIN PIN)",
    "READY",
}
SAFE_BOOT_PATTERNS = (
    re.compile(r"^IDENTITY DB: PASS identities=\d+ owners=\d+ active=\d+ revoked=\d+$"),
    re.compile(r"^SD: OK(?: \(\d+ MB\))?$"),
)
SAFE_DIAGNOSTIC = re.compile(r"^(SYSTEM INSPECT:|LINE INSPECT:|LINE PIPELINE:|SYSTEM DIAG END$)")


def next_line(port):
    raw = port.readline()
    if not raw:
        return None
    return raw.decode("utf-8", errors="replace").strip()


def main():
    retained = [
        "Production restoration passive verification",
        f"Started UTC: {datetime.now(timezone.utc).isoformat()}",
        f"Port: {PORT} at {BAUD} baud",
        "Reset: one normal 150 ms RTS pulse; DTR held false",
        "Only DIAG_SYSTEM sent; DIAG_HEAP_MIN not sent",
        "Serial retention: allowlisted boot health markers and safe SYSTEM/LINE summary lines only",
    ]
    boot = []
    discarded = 0
    ready = False

    port = serial.Serial()
    port.port = PORT
    port.baudrate = BAUD
    port.timeout = 0.25
    port.write_timeout = 2
    port.dtr = False
    port.rts = False
    port.open()
    try:
        port.dtr = False
        port.rts = True
        time.sleep(0.150)
        port.rts = False

        boot_deadline = time.monotonic() + 90
        while not ready and time.monotonic() < boot_deadline:
            line = next_line(port)
            if line is None:
                continue
            if line in SAFE_BOOT_EXACT or any(p.match(line) for p in SAFE_BOOT_PATTERNS):
                boot.append(line)
                if line == "READY":
                    ready = True
            else:
                discarded += 1
        if not ready:
            retained.append("Safe boot markers: " + " | ".join(boot))
            retained.append("RESULT: FAIL; READY marker not observed within 90 seconds")
            OUT.write_text("\n".join(retained) + "\n", encoding="utf-8")
            print(retained[-1])
            return 2

        time.sleep(1)
        port.write(b"DIAG_SYSTEM\n")
        port.flush()
        summary = []
        ended = False
        deadline = time.monotonic() + 20
        while not ended and time.monotonic() < deadline:
            line = next_line(port)
            if line is None:
                continue
            if line.startswith("NETWORK INSPECT:"):
                values = {
                    key: re.search(rf"\b{key}=(\d+)\b", line)
                    for key in ("ap_enabled", "mdns", "http")
                }
                if all(value is not None for value in values.values()):
                    summary.append(
                        "NETWORK HEALTH: "
                        + " ".join(f"{key}={value.group(1)}" for key, value in values.items())
                    )
                else:
                    discarded += 1
            elif SAFE_DIAGNOSTIC.match(line):
                summary.append(line)
                if line == "SYSTEM DIAG END":
                    ended = True
            else:
                discarded += 1

        retained.append("Safe boot markers: " + " | ".join(boot))
        retained.extend(summary)
        retained.append(f"Discarded non-allowlisted serial lines: {discarded}")
        required = {
            "GPIO22: LOCKED": "GPIO22 boot state",
            "ADMIN PIN STORE: OK": "Admin PIN store",
            "Touch calibration loaded": "touch calibration",
        }
        missing = [label for marker, label in required.items() if marker not in boot]
        system = next((line for line in summary if line.startswith("SYSTEM INSPECT:")), "")
        line_state = next((line for line in summary if line.startswith("LINE INSPECT:")), "")
        network_state = next((line for line in summary if line.startswith("NETWORK HEALTH:")), "")
        checks = {
            "configured_owner": bool(re.search(r"configured=1 owner=1\b", system)),
            "calibration_pin_locked_gpio": bool(
                re.search(r"calibration=1 pin_store=1 locked=1 gpio22=1\b", system)
            ),
            "wifi_ap_or_sta": bool(
                re.search(r"\bsta=1\b", system) or re.search(r"\bap_enabled=1\b", network_state)
            ),
            "line_configured_enabled": bool(re.search(r"configured=1 enabled=1\b", line_state)),
            "identity_owner_boot_marker": any(
                re.match(r"^IDENTITY DB: PASS identities=\d+ owners=[1-9]\d*\b", line)
                for line in boot
            ),
        }
        if missing:
            checks["boot_health_markers"] = False
        passed = ended and all(checks.values())
        retained.append("Checks: " + ", ".join(f"{key}={'PASS' if value else 'FAIL'}" for key, value in checks.items()))
        retained.append("RESULT: " + ("PASS" if passed else "FAIL" if ended else "INCOMPLETE; SYSTEM DIAG END not observed"))
        OUT.write_text("\n".join(retained) + "\n", encoding="utf-8")
        print("RESULT=" + ("PASS" if passed else "FAIL" if ended else "INCOMPLETE"))
        for line in boot:
            print("BOOT=" + line)
        for line in summary:
            print(line)
        print(retained[-2])
        return 0 if passed else 3 if ended else 4
    finally:
        port.close()


if __name__ == "__main__":
    sys.exit(main())
