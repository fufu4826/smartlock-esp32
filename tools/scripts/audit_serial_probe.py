"""Read-only USB audit probe. Never sends reset, credentials, or unlock commands."""
import csv
import io
import json
import re
import time
import zlib
import serial

port = serial.Serial()
port.port = 'COM6'
port.baudrate = 115200
port.timeout = 0.2
port.dtr = False
port.rts = False
port.open()
port.reset_input_buffer()
port.write(b'DIAG_AUDIT\n')
lines = []
deadline = time.monotonic() + 5
while time.monotonic() < deadline:
    line = port.readline().decode('ascii', 'replace').strip()
    if line:
        lines.append(line)
    if line == 'AUDIT CSV END':
        break
port.close()
status = next((line for line in lines if line.startswith('AUDIT STATUS:')), None)
assert status and 'enabled=1' in status and 'fault=0' in status, lines
start, end = lines.index('AUDIT CSV BEGIN'), lines.index('AUDIT CSV END')
rows = list(csv.DictReader(io.StringIO('\n'.join(lines[start+1:end]))))
assert 1 <= len(rows) <= 32
assert len({row['event_id'] for row in rows}) == len(rows)
for raw, row in zip(lines[start+2:end], rows):
    assert re.fullmatch('[0-9a-f]{40}', row['event_id'])
    assert not row['user_id'] or re.fullmatch('U[0-9]{6}', row['user_id'])
    assert not row['device_id'] or re.fullmatch('D[0-9]{6}', row['device_id'])
    data, crc = raw.rsplit(',', 1)
    assert zlib.crc32(data.encode('ascii')) == int(crc, 16)
    assert row['time_quality'] in ('NTP', 'UPTIME')
    assert bool(row['timestamp']) == (row['time_quality'] == 'NTP')
print(status)
print(json.dumps({'validated_records': len(rows), 'actions': [r['action'] for r in rows],
                  'event_ids': [r['event_id'] for r in rows],
                  'secrets_in_schema': False}, indent=2))
