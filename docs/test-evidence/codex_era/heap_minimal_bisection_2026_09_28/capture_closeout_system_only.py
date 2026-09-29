from datetime import datetime, timezone
from pathlib import Path
import re
import sys
import time

import serial


OUT = Path(__file__).resolve().parent / "closeout_latest_diagsystem.txt"
PORT = "COM6"
BAUD = 115200


def main():
    kept = [
        "Closeout current SYSTEM-only capture",
        f"Captured UTC: {datetime.now(timezone.utc).isoformat()}",
        f"Port: {PORT}, {BAUD} baud; no reset; DTR/RTS false; no control-line pulse",
        "Exactly one existing read-only command sent: DIAG_SYSTEM",
        "Retained fields: SYSTEM, sanitized NETWORK booleans, HEAP, LINE status/counters only",
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
        # Drop any pre-existing output without retaining it.
        drain_deadline = time.monotonic() + 0.5
        while time.monotonic() < drain_deadline:
            port.readline()

        port.write(b"DIAG_SYSTEM\n")
        port.flush()
        ended = False
        deadline = time.monotonic() + 20
        while not ended and time.monotonic() < deadline:
            raw = port.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="replace").strip()
            if line.startswith(("SYSTEM INSPECT:", "HEAP:", "LINE INSPECT:", "LINE PIPELINE:")):
                kept.append(line)
            elif line.startswith("NETWORK INSPECT:"):
                sta_ip = re.search(r"\bsta_ip=([^ ]+)", line)
                values = {
                    key: re.search(rf"\b{key}=(\d+)\b", line)
                    for key in ("ap_enabled", "ap_clients", "mdns", "http")
                }
                if sta_ip is not None and all(values.values()):
                    sta = int(sta_ip.group(1) not in ("0.0.0.0", ""))
                    kept.append(
                        "NETWORK HEALTH: "
                        + " ".join(
                            [f"sta_connected={sta}"]
                            + [f"{key}={value.group(1)}" for key, value in values.items()]
                        )
                    )
            elif line == "SYSTEM DIAG END":
                kept.append(line)
                ended = True
        kept.append("RESULT: " + ("PASS" if ended else "INCOMPLETE; SYSTEM DIAG END not observed"))
        OUT.write_text("\n".join(kept) + "\n", encoding="utf-8")
        print(kept[-1])
        for line in kept[5:-1]:
            print(line)
        return 0 if ended else 2
    finally:
        port.close()


if __name__ == "__main__":
    sys.exit(main())
