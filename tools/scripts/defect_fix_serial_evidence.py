"""USB read-only diagnostics; optional normal EN reboot, never factory reset."""
import argparse
from pathlib import Path
import time
import serial

p=argparse.ArgumentParser();p.add_argument('--port',default='COM6');p.add_argument('--output',required=True)
p.add_argument('--reset',action='store_true');p.add_argument('--settle',type=float,default=25)
a=p.parse_args();chunks=[]
with serial.Serial(port=None,baudrate=115200,timeout=0.15) as port:
    port.dtr=False;port.rts=False;port.port=a.port;port.open()
    if a.reset:
        port.rts=True;time.sleep(0.15);port.rts=False
    def collect(seconds):
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            data=port.read(port.in_waiting or 1)
            if data:chunks.append(data)
    collect(a.settle)
    for command in [b'DIAG_AUTH\n',b'DIAG_SYSTEM\n',b'DIAG_AUDIT\n',b'DIAG_AUTH\n']:
        port.write(command);port.flush();collect(4)
text='\n'.join(line.rstrip() for line in b''.join(chunks).decode('utf-8',errors='replace').splitlines())+'\n'
Path(a.output).write_text(text,encoding='utf-8',newline='\n')
for line in text.splitlines():
    if any(marker in line for marker in ('INSPECT','SYSTEM SESSIONS','IDENTITY META','AUDIT STATUS','AUDIT HISTORY','HEAP:','GPIO22:','STA:','READY','panic','watchdog')):
        print(line)
