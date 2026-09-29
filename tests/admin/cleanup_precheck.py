"""Read-only normal-boot preflight for removing legacy maintenance code."""
from pathlib import Path
import serial,time
s=serial.Serial();s.port='COM6';s.baudrate=115200;s.dtr=False;s.rts=False;s.timeout=.2;s.open();s.rts=True;time.sleep(.15);s.rts=False
data=b'';end=time.time()+11
while time.time()<end:data+=s.read(4096)
s.write(b'DIAG_AUTH\n');end=time.time()+3
while time.time()<end:data+=s.read(4096)
s.close();text=data.decode('utf-8','replace')
p=Path('docs/phase_reports/evidence/product_cleanup');p.mkdir(parents=True,exist_ok=True)
(p/'before-boot.txt').write_text(text,encoding='utf-8')
for value in ('RESTORE GENERATION: legacy_current','ADMIN PIN STORE: OK','GPIO22: LOCKED','identities=4 owners=1 verifiers=4 OWNER_VERIFIER_PRESERVED=1','STA: CONNECTED'):
 assert value in text,value
print('Legacy generation confirmed; Owner/four verifiers/PIN store/STA/locked boot PASS; no state erased')
