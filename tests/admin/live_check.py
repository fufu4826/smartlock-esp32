"""Explicit normal reboot + read-only metadata and unauthenticated PIN-route checks."""
from pathlib import Path
import serial,time,urllib.request,urllib.error,json
out=Path('docs/phase_reports/evidence/admin_pin');out.mkdir(parents=True,exist_ok=True)
s=serial.Serial();s.port='COM6';s.baudrate=115200;s.dtr=False;s.rts=False;s.timeout=.2;s.open()
# EN pulse only; GPIO0 remains released. Never Factory Reset or calibration.
s.rts=True;time.sleep(.15);s.rts=False
data=b'';end=time.time()+12
while time.time()<end:data+=s.read(4096)
s.write(b'DIAG_AUTH\n');end=time.time()+4
while time.time()<end:data+=s.read(4096)
s.close();text=data.decode('utf-8','replace');(out/'boot-auth.txt').write_text(text,encoding='utf-8');print(text)
for expected in ('GPIO22: LOCKED','ADMIN PIN STORE: OK','SD: OK','Touch calibration loaded','STA: CONNECTED','CANONICAL:','MDNS: OK','identities=4 owners=1 verifiers=4 OWNER_VERIFIER_PRESERVED=1'):
 assert expected in text,expected
assert 'panic' not in text.lower() and 'watchdog' not in text.lower()
base='http://192.168.1.179'
assert urllib.request.urlopen(base+'/health',timeout=5).read().strip()==b'ok'
page=urllib.request.urlopen(base+'/manage',timeout=5).read().decode('utf-8');assert 'adminPinForm' in page
statuses=[]
for body,expected in [('token='+'0'*64+'&current=0000&next=0000&confirm=0000',403),('token='+'0'*64,400)]:
 r=urllib.request.Request(base+'/api/system/admin-pin',data=body.encode(),headers={'Content-Type':'application/x-www-form-urlencoded'})
 try:status=urllib.request.urlopen(r,timeout=5).status
 except urllib.error.HTTPError as e:status=e.code
 assert status==expected,(status,expected);statuses.append(status)
(out/'http.json').write_text(json.dumps({'numericHealth':'PASS','newPageServed':'PASS','unauthorizedPinChange':statuses[0],'malformedPinChange':statuses[1]},indent=2),encoding='utf-8')
print('Admin live pre-check PASS; no PIN verification/change/unlock/reset request issued')
