from datetime import datetime, timezone
from pathlib import Path
import re
import sys
import time

import serial


OUT = Path(__file__).resolve().parent / "current_provenance_capture.txt"
PORT = "COM6"
BAUD = 115200


def read_line(port):
    raw = port.readline()
    if not raw:
        return None
    return raw.decode("utf-8", errors="replace").strip()


def wait_summary(port, end_marker, timeout_s, accept):
    kept = []
    deadline = time.monotonic() + timeout_s
    ended = False
    while not ended and time.monotonic() < deadline:
        line = read_line(port)
        if not line:
            continue
        if line == end_marker:
            kept.append(line)
            ended = True
        else:
            safe = accept(line)
            if safe:
                kept.append(safe)
    return kept, ended


def auth_line(line):
    match = re.match(
        r"^AUTH INSPECT: valid=(\d+) identities=(\d+) owners=(\d+) verifiers=(\d+) OWNER_VERIFIER_PRESERVED=(\d+)",
        line,
    )
    if not match:
        return None
    names = ("valid", "identities", "owners", "verifiers", "OWNER_VERIFIER_PRESERVED")
    return "AUTH INSPECT: " + " ".join(
        f"{name}={value}" for name, value in zip(names, match.groups())
    )


def system_line(line):
    if line.startswith(("SYSTEM INSPECT:", "LINE INSPECT:", "LINE PIPELINE:")):
        return line
    if line.startswith("NETWORK INSPECT:"):
        sta_ip = re.search(r"\bsta_ip=([^ ]+)", line)
        values = {
            key: re.search(rf"\b{key}=(\d+)\b", line)
            for key in ("ap_enabled", "ap_clients", "mdns", "http")
        }
        if sta_ip is None or not all(values.values()):
            return None
        sta_connected = int(sta_ip.group(1) not in ("0.0.0.0", ""))
        return "NETWORK HEALTH: " + " ".join(
            [f"sta_connected={sta_connected}"]
            + [f"{key}={value.group(1)}" for key, value in values.items()]
        )
    return None


def main():
    kept = [
        "Current production provenance capture",
        f"Started UTC: {datetime.now(timezone.utc).isoformat()}",
        f"Port: {PORT} at {BAUD} baud",
        "Reset: none; DTR and RTS were set false before open and were not pulsed",
        "Commands: DIAG_AUTH once, then DIAG_SYSTEM once",
        "Retention: sanitized AUTH counts and safe SYSTEM/NETWORK/LINE state only",
    ]
    port = serial.Serial()
    port.port = PORT
    port.baudrate = BAUD
    port.timeout = 0.25
    port.write_timeout = 2
    port.dtr = False
    port.rts = False
    port.open()
    try:
        # Drain any already-buffered boot/status text without retaining it.
        drain_deadline = time.monotonic() + 1
        while time.monotonic() < drain_deadline:
            read_line(port)

        port.write(b"DIAG_AUTH\n")
        port.flush()
        auth, auth_end = wait_summary(port, "AUTH INSPECT END", 15, auth_line)
        kept.extend(auth)
        if not auth_end:
            kept.append("AUTH RESULT: timeout")

        port.write(b"DIAG_SYSTEM\n")
        port.flush()
        system, system_end = wait_summary(port, "SYSTEM DIAG END", 20, system_line)
        kept.extend(system)
        if not system_end:
            kept.append("SYSTEM RESULT: timeout")

        kept.append("RESULT: " + ("PASS" if auth_end and system_end else "INCOMPLETE"))
        OUT.write_text("\n".join(kept) + "\n", encoding="utf-8")
        print(kept[-1])
        for line in auth + system:
            print(line)
        return 0 if auth_end and system_end else 2
    finally:
        port.close()


if __name__ == "__main__":
    sys.exit(main())
