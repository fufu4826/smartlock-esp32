"""Explicit USB-only read-only backup fixture/crypto diagnostic."""
import argparse,time,serial
p=argparse.ArgumentParser();p.add_argument('--timeout',type=int,default=90);a=p.parse_args()
s=serial.Serial();s.port='COM6';s.baudrate=115200;s.timeout=.2;s.dtr=False;s.rts=False;s.open()
s.reset_input_buffer();s.write(b'DIAG_BACKUP\n');deadline=time.monotonic()+a.timeout;done=False
while time.monotonic()<deadline:
 line=s.readline().decode('utf-8','replace').strip()
 if line:print(line,flush=True)
 if line=='BACKUP SELFTEST END':done=True;break
s.close()
if not done:raise SystemExit('Backup diagnostic did not complete; no production state was restored')
