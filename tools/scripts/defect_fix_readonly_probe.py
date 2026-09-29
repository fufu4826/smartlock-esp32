"""Non-authorizing live regression; no credential discovery or state-changing grants."""
import argparse
import http.client
import json
from pathlib import Path
import select
import socket
import time
from urllib.parse import urlencode

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--host',required=True)
    parser.add_argument('--output',required=True)
    args=parser.parse_args()
    rows=[]
    def request(path,fields=None,expected=200):
        connection=http.client.HTTPConnection(args.host,80,timeout=8)
        start=time.monotonic()
        body=urlencode(fields) if fields is not None else None
        connection.request('POST' if fields is not None else 'GET',path,body,
                           {'Content-Type':'application/x-www-form-urlencoded'} if body is not None else {})
        response=connection.getresponse();data=response.read();connection.close()
        row={'path':path.split('?')[0],'status':response.status,'expected':expected,
             'seconds':round(time.monotonic()-start,3),'pass':response.status==expected}
        if path in ('/health','/api/status'):row['body']=data.decode('utf-8')
        rows.append(row)
        if not row['pass']:raise RuntimeError(str(row))
        return data
    try:
        request('/health');request('/api/status')
        for path in ['/api/system/backup','/api/system/restore','/api/system/ota','/api/system/firmware']:
            request(path,{},404)
        request('/verify-owner',expected=410)
        request('/api/manage/state',{'token':'0'*64},403)
        request('/api/manage/state',{'token':'0'*506},403) # 512-byte accepted parser body
        request('/api/setup/complete',{'x':'0'*898},409) # 900-byte Setup body, configured board refuses mutation
        request('/api/manage/enroll',{'token':'0'*64,'name':'NON_AUTHORIZING_FIXTURE','role':'User'},403)
        request('/api/manage/revoke',{'token':'0'*64,'deviceId':'D999999'},403)
        request('/api/network/status',{'token':'0'*64},403)
        request('/api/system/admin-pin',{'token':'0'*64,'current':'invalid','next':'invalid','confirm':'invalid'},403)
        for path in ['/api/logs/status','/api/logs/segments','/api/logs/export']:
            request(path,{'token':'0'*64},403)
        request('/api/enroll/check?session='+'0'*64,expected=403)
        request('/api/enroll/complete',{'session':'0'*64,'credential':'0'*64},403)
        request('/api/registration/reconcile',{'kind':'setup','credential':'0'*64},403)
        request('/api/access/request',{'session':'0'*64,'deviceId':'D000001','credential':'0'*64},403)
        # One gentle 512-byte declared body, 4 bytes/sec. Stop as soon as firmware closes it.
        with socket.create_connection((args.host,80),timeout=4) as stream:
            start=time.monotonic()
            stream.sendall((f'POST /api/manage/state HTTP/1.1\r\nHost: {args.host}\r\n'
                           'Content-Type: application/x-www-form-urlencoded\r\nContent-Length: 512\r\nConnection: close\r\n\r\n').encode())
            closed=False;sent=0
            while time.monotonic()-start<8:
                readable,_,_=select.select([stream],[],[],0.25)
                if readable:
                    if not stream.recv(1024):closed=True;break
                try:stream.sendall(b'x');sent+=1
                except (BrokenPipeError,ConnectionResetError):closed=True;break
            elapsed=time.monotonic()-start
        rows.append({'check':'single slow body deadline','seconds':round(elapsed,3),'bytes_sent':sent,
                     'closed':closed,'pass':closed and 4<=elapsed<=6.5})
        if not rows[-1]['pass']:raise RuntimeError(str(rows[-1]))
        request('/health')
        if rows[-1]['seconds']>2:raise RuntimeError('Main loop did not resume promptly')
    finally:
        Path(args.output).write_text(json.dumps(rows,indent=2),encoding='utf-8')
    print(json.dumps({'checks':len(rows),'all_pass':all(r['pass'] for r in rows)}))

if __name__=='__main__':main()
