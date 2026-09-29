"""Focused, read-only production cleanup boot and HTTP checks; no credentials."""
from pathlib import Path
import json, time, urllib.request, urllib.error
import serial

out = Path('docs/phase_reports/evidence/product_cleanup')
out.mkdir(parents=True, exist_ok=True)
s = serial.Serial(); s.port = 'COM6'; s.baudrate = 115200
s.dtr = False; s.rts = False; s.timeout = .2; s.open()
s.rts = True; time.sleep(.15); s.rts = False
data = b''
end = time.time() + 12
while time.time() < end: data += s.read(4096)
s.write(b'DIAG_AUTH\n'); end = time.time() + 3
while time.time() < end: data += s.read(4096)
s.close()
text = data.decode('utf-8', 'replace')
(out/'after-boot.txt').write_text(text.replace('\r', ''), encoding='utf-8')
for value in ('GPIO22: LOCKED', 'Configured: YES', 'ADMIN PIN STORE: OK',
              'identities=4 owners=1 verifiers=4 OWNER_VERIFIER_PRESERVED=1',
              'STA: CONNECTED', 'MDNS: OK'):
    assert value in text, value
for identity in ('D000001 name=gugy role=0 status=0', 'D000002 name=boom role=2 status=0',
                 'D000003 name=Root role=2 status=0', 'D000004 name=Ki role=2 status=0'):
    assert identity in text, identity
assert 'Guru Meditation' not in text and 'watchdog' not in text.lower()
base = 'http://192.168.1.179'
results = []
def request(path, method='GET', body=None):
    req = urllib.request.Request(base+path, data=body, method=method)
    try:
        with urllib.request.urlopen(req, timeout=8) as r: status, data = r.status, r.read()
    except urllib.error.HTTPError as e: status, data = e.code, e.read()
    results.append({'path':path, 'method':method, 'status':status})
    return status, data
assert request('/health')[1].strip() == b'ok'
status, page = request('/manage')
assert status == 200 and b'adminPinForm' in page
for removed in (b'ownerMaintenance', b'/api/system/backup', b'/api/system/ota', b'/api/system/restore'):
    assert removed not in page, removed
paths = ['/api/system/backup', '/api/system/restore/validate', '/api/system/restore/activate',
         '/api/system/ota/prepare', '/api/system/ota/start', '/api/system/ota/chunk',
         '/api/system/ota/finish', '/api/system/ota/result', '/api/system/ota/current']
for path in paths:
    for method in ('GET', 'POST'):
        assert request(path, method, b'token=invalid' if method == 'POST' else None)[0] == 404
assert request('/api/system/admin-pin', 'POST', b'token=invalid&current=0000&next=0000&confirm=0000')[0] == 403
(out/'http.json').write_text(json.dumps(results, indent=2)+'\n', encoding='utf-8')
print('Cleanup boot preservation and 18 removed-endpoint checks PASS; PIN route unauthorized rejection PASS')
