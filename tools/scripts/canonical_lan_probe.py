"""Read-only LAN DNS/mDNS/HTTP proof; no credential or lock mutation."""
import json,socket,struct,sys,urllib.request
from pathlib import Path
def question(name, multicast=False):
    labels = b''.join(bytes([len(label)]) + label.encode('ascii') for label in name.split('.')) + b'\0'
    return struct.pack('!6H', 0 if multicast else 0x4353, 0 if multicast else 0x100,
                       1, 0, 0, 0) + labels + struct.pack('!HH', 1, 0x8001 if multicast else 1)

def skip_name(packet, offset):
    for _ in range(128):
        length = packet[offset]
        if length & 0xc0 == 0xc0:
            return offset + 2
        offset += 1
        if not length:
            return offset
        offset += length
    raise ValueError('DNS name too long')

def addresses(packet):
    _, flags, qd, an, ns, ar = struct.unpack('!6H', packet[:12])
    assert flags & 0x8000
    offset = 12
    for _ in range(qd):
        offset = skip_name(packet, offset) + 4
    found = []
    for _ in range(an + ns + ar):
        offset = skip_name(packet, offset)
        kind, cls, ttl, size = struct.unpack('!HHIH', packet[offset:offset + 10])
        offset += 10
        if kind == 1 and size == 4:
            found.append(socket.inet_ntoa(packet[offset:offset + size]))
        offset += size
    return found

def get(url):
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    with opener.open(url, timeout=5) as response:
        return response.read().decode('utf-8')


host=sys.argv[1]
ip=sys.argv[2]
results={}
results['os_resolution']=sorted({a[4][0] for a in socket.getaddrinfo(host,80,socket.AF_INET)})
assert ip in results['os_resolution']
for label,origin in [('canonical','http://'+host),('numeric','http://'+ip)]:
 assert get(origin+'/health')=='ok\n'
 status=json.loads(get(origin+'/api/status'))
 assert status['canonicalHost']==host
 page=get(origin+'/setup')
 assert 'lang="th"' in page and 'charset="UTF-8"' in page
 results[label]={'health':'PASS','configured':status['configured'],'thai_setup':'PASS'}
with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as route:
 route.connect((ip,53));local_ip=route.getsockname()[0]
with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as udp:
 udp.bind((local_ip,0));udp.setsockopt(socket.IPPROTO_IP,socket.IP_MULTICAST_IF,socket.inet_aton(local_ip))
 udp.settimeout(5);udp.sendto(question(host,True),('224.0.0.251',5353))
 results['sta_mdns']=addresses(udp.recv(4096));assert ip in results['sta_mdns']
with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as udp:
 udp.settimeout(5);udp.sendto(question(host),(ip,53))
 results['ap_dns_via_sta']=addresses(udp.recv(4096));assert '192.168.4.1' in results['ap_dns_via_sta']
results['ap_client_association']='NOT TESTED; protected AP profile unavailable'
print(json.dumps(results,indent=2))
Path('.pio/canonical-lan-result.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
