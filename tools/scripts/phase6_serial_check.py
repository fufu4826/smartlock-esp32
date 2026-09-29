import serial
import time
import sys

sys.stdout.reconfigure(errors="replace")

port = serial.Serial("COM6", 115200, timeout=0.4)
port.dtr = False
port.rts = True
time.sleep(0.1)
port.rts = False
end = time.monotonic() + (float(sys.argv[1]) if len(sys.argv) > 1 else 12)
lines = []
while time.monotonic() < end:
    line = port.readline().decode("utf-8", "replace").strip()
    if line:
        lines.append(line)
port.close()
for line in lines[-60:]:
    print(line)
