"""LAN health and negative API checks only. Never provisions or unlocks a device."""
import sys
import time
import urllib.request
import urllib.parse
import urllib.error

host = sys.argv[1] if len(sys.argv) > 1 else '192.168.1.179'
opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
base = 'http://' + host
with opener.open(base + '/health', timeout=8) as response:
    assert response.read().strip() == b'ok'
print('LAN health PASS; this request verifies LAN reachability for AP shutdown')
# Allow the network manager's response grace to finish before probing again.
time.sleep(4)
with opener.open(base + '/health', timeout=8) as response:
    assert response.read().strip() == b'ok'
print('LAN health after AP shutdown grace PASS')
invalid = '0' * 64
checks = [
    ('/api/manage/users', {'token':invalid,'name':'NeverCreated','role':'Owner'}),
    ('/api/manage/enroll', {'token':invalid,'userId':'U000001','deviceName':'NeverCreated'}),
    ('/api/manage/revoke', {'token':invalid,'deviceId':'D000001'}),
    ('/api/manage/user-status', {'token':invalid,'userId':'U000001','enabled':'0'}),
    ('/api/enroll/complete', {'session':invalid,'credential':'1'*64,'deviceName':'NeverCreated'}),
    ('/api/access/request', {'session':invalid,'deviceId':'D000001','credential':'1'*64}),
]
for path, fields in checks:
    req = urllib.request.Request(base+path, urllib.parse.urlencode(fields).encode(),
                                 {'Content-Type':'application/x-www-form-urlencoded'})
    try:
        opener.open(req, timeout=8)
        raise AssertionError('Unexpected acceptance: '+path)
    except urllib.error.HTTPError as error:
        assert error.code == 403, (path,error.code)
    print(path+': denied PASS')
with opener.open(base+'/manage', timeout=8) as response:
    text = response.read().decode('utf-8')
    assert 'id="toggleUserForm"' in text and 'id="enrollQr"' in text
    assert 'LAN users checkpoint' in text and '\ufffd' not in text
print('Current Thai Users/QR page delivered PASS')
