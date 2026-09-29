"""Read-only Phase13 HTTP negative tests. Never supplies a real credential."""
import argparse, socket, urllib.request, urllib.error, time
p=argparse.ArgumentParser();p.add_argument('--host',default='192.168.1.179');a=p.parse_args();base='http://'+a.host
count=0
def post(path,body,content='application/x-www-form-urlencoded',expected=(400,403,429)):
 global count
 q=urllib.request.Request(base+path,data=body.encode(),headers={'Content-Type':content})
 try:
  with urllib.request.urlopen(q,timeout=4) as r:status=r.status
 except urllib.error.HTTPError as e:status=e.code
 assert status in expected,(path,status)
 count+=1
 return status
statuses=[post('/api/manage/login','session='+'0'*64+'&deviceId=D000001&credential='+'0'*64) for _ in range(10)]
assert statuses[-1]==429, statuses
# Bounds are enforced before form decoding. Close without a response is an
# intentional parser rejection; a response must be an error, never success.
def raw(request):
 global count
 with socket.create_connection((a.host,80),timeout=4) as s:
  s.settimeout(4);s.sendall(request);data=b''
  try:data=s.recv(512)
  except (ConnectionResetError,socket.timeout):pass
 assert not data or data.startswith((b'HTTP/1.1 4',b'HTTP/1.0 4')),(data[:100])
 count+=1
raw(b'POST /api/manage/login HTTP/1.1\r\nHost: test\r\nContent-Length: 99999\r\n\r\n')
raw(b'POST /api/manage/login HTTP/1.1\r\nHost: test\r\nContent-Length: -1\r\n\r\n')
raw(b'POST /api/manage/login HTTP/1.1\r\nHost: test\r\nContent-Length: 1\r\nContent-Length: 1\r\n\r\nx')
raw(b'POST /api/manage/login HTTP/1.1\r\nHost: test\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n')
raw(b'GET /'+b'x'*1100+b' HTTP/1.1\r\nHost: test\r\n\r\n')
raw(b'GET /health HTTP/1.1\r\nHost: test\r\nX-Long: '+b'x'*1100+b'\r\n\r\n')
raw(b'GET /health?'+b'&'.join(b'a=1' for _ in range(20))+b' HTTP/1.1\r\nHost: test\r\n\r\n')
raw(b'POST /api/system/ota/chunk HTTP/1.1\r\nHost: test\r\nContent-Length: 2301\r\n\r\n')
raw(b'POST /api/system/restore/activate HTTP/1.1\r\nHost: test\r\nContent-Length: 25001\r\n\r\n')
assert urllib.request.urlopen(base+'/health',timeout=4).read().strip()==b'ok'
print(f'PASS: {count} read-only HTTP rejection checks; health remains ok')
