from datetime import datetime, timezone
from pathlib import Path
import re
import sys
import time

import serial


OUT = Path(__file__).resolve().parent / "production_restore_late_status.txt"
PORT = "COM6"
BAUD = 115200
WAIT_SECONDS = 120


def next_line(port):
    raw = port.readline()
    if not raw:
        return None
    return raw.decode("utf-8", errors="replace").strip()


def main():
    kept = [
        "Production later passive status",
        f"Started UTC: {datetime.now(timezone.utc).isoformat()}",
        f"Port: {PORT} at {BAUD} baud; no reset or control-line pulse",
        f"Passive startup wait before command: {WAIT_SECONDS} seconds",
        "Command sent exactly once: DIAG_SYSTEM; DIAG_HEAP_MIN not sent",
        "Serial retention: sanitized STA event and safe SYSTEM/LINE summary only",
    ]
    sta_event = "not observed during wait"
    discarded = 0

    port = serial.Serial()
    port.port = PORT
    port.baudrate = BAUD
    port.timeout = 0.25
    port.write_timeout = 2
    port.dtr = False
    port.rts = False
    port.open()
    try:
        deadline = time.monotonic() + WAIT_SECONDS
        while time.monotonic() < deadline:
            line = next_line(port)
            if line is None:
                continue
            if line.startswith("STA: CONNECTED IP "):
                sta_event = "connected event observed; IP withheld"
            elif line.startswith("STA: FAILED"):
                sta_event = "failure event observed; details withheld"
            else:
                discarded += 1

        port.write(b"DIAG_SYSTEM\n")
        port.flush()
        summary = []
        ended = False
        command_deadline = time.monotonic() + 20
        while not ended and time.monotonic() < command_deadline:
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
            elif line.startswith(("SYSTEM INSPECT:", "LINE INSPECT:", "LINE PIPELINE:")):
                summary.append(line)
            elif line == "SYSTEM DIAG END":
                summary.append(line)
                ended = True
            else:
                discarded += 1

        kept.append("STA startup event: " + sta_event)
        kept.extend(summary)
        kept.append(f"Discarded non-allowlisted serial lines: {discarded}")
        kept.append("RESULT: " + ("PASS" if ended else "INCOMPLETE; SYSTEM DIAG END not observed"))
        OUT.write_text("\n".join(kept) + "\n", encoding="utf-8")
        print("RESULT=" + ("PASS" if ended else "INCOMPLETE"))
        print("STA_EVENT=" + sta_event)
        for line in summary:
            print(line)
        return 0 if ended else 2
    finally:
        port.close()


if __name__ == "__main__":
    sys.exit(main())
