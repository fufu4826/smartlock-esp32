"""Read-only USB identity metadata; never outputs a verifier or browser credential."""
import argparse
import time
import serial

parser = argparse.ArgumentParser()
parser.add_argument('--port', default='COM6')
args = parser.parse_args()
port = serial.Serial()
port.port = args.port
port.baudrate = 115200
port.timeout = 0.2
port.dtr = False
port.rts = False
port.open()
try:
    port.reset_input_buffer()
    port.write(b'DIAG_AUTH\n')
    end = time.monotonic() + 8
    complete = False
    while time.monotonic() < end:
        line = port.readline().decode('utf-8', 'replace').strip()
        if line:
            print(line)
        if line == 'AUTH INSPECT END':
            complete = True
            break
    if not complete:
        raise SystemExit('Identity diagnostic did not complete')
finally:
    port.close()
