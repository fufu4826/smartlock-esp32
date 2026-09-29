from datetime import datetime, timezone
from pathlib import Path
import re
import sys
import time

import serial


OUT = Path(__file__).resolve().parent / "minimal_stable_idle_capture.txt"
PORT = "COM6"
BAUD = 115200
IDLE_SECONDS = 180
SAFE_BOOT = {
    "BOOT",
    "SmartLock core product (USB updates)",
    "LockController: LOCKED",
    "GPIO22: LOCKED",
    "LOCK TIMER: OK",
    "LINE: READY (Owner Management + ADMIN PIN)",
    "READY",
}
LINE_CAPTURE = re.compile(r"^(LINE INSPECT:|LINE PIPELINE:|SYSTEM DIAG END$)")


def next_line(port):
    raw = port.readline()
    if not raw:
        return None
    return raw.decode("utf-8", errors="replace").strip()


def main():
    lines = [
        "Minimal heap probe capture",
        f"Started UTC: {datetime.now(timezone.utc).isoformat()}",
        f"Port: {PORT} at {BAUD} baud",
        "Reset: one 150 ms RTS pulse; DTR held false",
        f"Stable-idle wait: {IDLE_SECONDS} seconds after READY",
        "Command order: DIAG_HEAP_MIN once, then DIAG_SYSTEM once",
        "Serial retention: safe boot markers, the raw DIAG_HEAP_MIN line, and LINE INSPECT/PIPELINE plus SYSTEM DIAG END only",
    ]
    boot_markers = []
    discarded = 0
    ready_at = None

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
        while ready_at is None and time.monotonic() < boot_deadline:
            line = next_line(port)
            if line is None:
                continue
            if line in SAFE_BOOT:
                boot_markers.append(line)
                if line == "READY":
                    ready_at = time.monotonic()
            else:
                discarded += 1
        if ready_at is None:
            lines.append("RESULT: FAIL; READY marker not observed within 90 seconds")
            lines.append("Safe boot markers: " + " | ".join(boot_markers))
            OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
            print(lines[-2])
            return 2

        while time.monotonic() - ready_at < IDLE_SECONDS:
            line = next_line(port)
            if line is None:
                continue
            if line in SAFE_BOOT and line not in boot_markers:
                boot_markers.append(line)
            else:
                discarded += 1

        port.write(b"DIAG_HEAP_MIN\n")
        port.flush()
        metric_line = None
        metric_deadline = time.monotonic() + 15
        while metric_line is None and time.monotonic() < metric_deadline:
            line = next_line(port)
            if line is None:
                continue
            if line.startswith("DIAG_HEAP_MIN "):
                metric_line = line
            else:
                discarded += 1
        if metric_line is None:
            lines.append("RESULT: FAIL; DIAG_HEAP_MIN response not observed")
            lines.append("Safe boot markers: " + " | ".join(boot_markers))
            OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
            print(lines[-2])
            return 3

        lines.append("Safe boot markers: " + " | ".join(boot_markers))
        lines.append("Raw metric line: " + metric_line)
        integrity = re.search(r"\bintegrity=(PASS|FAIL)\b", metric_line)
        if integrity is None:
            lines.append("RESULT: FAIL; integrity field missing")
            OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
            print(lines[-1])
            print("METRIC=" + metric_line)
            return 4
        if integrity.group(1) == "FAIL":
            lines.append("RESULT: HEAP INTEGRITY FAIL; DIAG_SYSTEM not sent")
            lines.append(f"Discarded non-allowlisted serial lines: {discarded}")
            OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
            print("HEAP_INTEGRITY_FAIL")
            print("METRIC=" + metric_line)
            return 5

        port.write(b"DIAG_SYSTEM\n")
        port.flush()
        line_diag = []
        diag_deadline = time.monotonic() + 20
        ended = False
        while not ended and time.monotonic() < diag_deadline:
            line = next_line(port)
            if line is None:
                continue
            if LINE_CAPTURE.match(line):
                line_diag.append(line)
                if line == "SYSTEM DIAG END":
                    ended = True
            else:
                discarded += 1

        lines.extend(line_diag)
        lines.append(f"Discarded non-allowlisted serial lines: {discarded}")
        lines.append("RESULT: PASS" if ended else "RESULT: INCOMPLETE; SYSTEM DIAG END not observed")
        OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
        print("RESULT=" + ("PASS" if ended else "INCOMPLETE"))
        print("METRIC=" + metric_line)
        for item in line_diag:
            print(item)
        return 0 if ended else 6
    finally:
        port.close()


if __name__ == "__main__":
    sys.exit(main())
