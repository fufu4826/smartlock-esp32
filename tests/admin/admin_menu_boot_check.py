"""Read-only normal boot check for the Admin menu regression; no PIN input."""
from pathlib import Path
import serial, time, urllib.request

out=Path('docs/phase_reports/evidence/admin_menu_fix')
out.mkdir(parents=True,exist_ok=True)
s=serial.Serial();s.port='COM6';s.baudrate=115200;s.dtr=False;s.rts=False;s.timeout=.2
s.open();s.rts=True;time.sleep(.15);s.rts=False
data=b'';end=time.time()+12
while time.time()<end:data+=s.read(4096)
s.write(b'DIAG_AUTH\n');end=time.time()+3
while time.time()<end:data+=s.read(4096)
s.close();text=data.decode('utf-8','replace').replace('\r','')
(out/'boot.txt').write_text(text,encoding='utf-8')
for value in ('GPIO22: LOCKED','Configured: YES','Owner exists: YES','ADMIN PIN STORE: OK',
              'Touch calibration loaded','SD: OK','STA: CONNECTED IP 192.168.1.179',
              'MDNS: OK','AUDIT: READY',
              'identities=4 owners=1 verifiers=4 OWNER_VERIFIER_PRESERVED=1'):
    assert value in text,value
for identity in ('D000001 name=gugy role=0 status=0','D000002 name=boom role=2 status=0',
                 'D000003 name=Root role=2 status=0','D000004 name=Ki role=2 status=0'):
    assert identity in text,identity
assert 'Guru Meditation' not in text and 'watchdog' not in text.lower()
with urllib.request.urlopen('http://192.168.1.179/health',timeout=8) as r:
    assert r.status==200 and r.read().strip()==b'ok'
print('Focused Admin fix boot preservation/GPIO22 locked/numeric health PASS; no unlock or PIN attempt')
