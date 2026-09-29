"""Read-only AP name/HTTP probe, restoring the PC's original Wi-Fi profile.

No Setup, credential, SD, NVS, or lock mutation is performed.
"""
import json
import re
import socket
import struct
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

def wlan(*args):
    return subprocess.run(['netsh', 'wlan', *args], capture_output=True,
                          text=True, errors='replace', timeout=10)

def connected_profile():
    output = wlan('show', 'interfaces').stdout
    match = re.search(r'^\s*Profile\s*:\s*(.+)$', output, re.M)
    return match.group(1).strip() if match else None

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

host = sys.argv[1]
assert re.fullmatch(r'smartlock-[0-9a-f]{12}\.local', host)
original = connected_profile()
assert original and original != 'Codex-SmartLock-Canonical-Probe', 'Original profile unavailable'
temporary = 'Codex-SmartLock-Canonical-Probe'
assert temporary not in wlan('show', 'profiles').stdout, 'Probe profile already exists'
profile_path = Path('.pio/canonical-probe-wifi.xml').resolve()
profile_path.write_text('''<?xml version="1.0"?>
<WLANProfile xmlns="http://www.microsoft.com/networking/WLAN/profile/v1">
<name>Codex-SmartLock-Canonical-Probe</name><SSIDConfig><SSID><name>SmartLock-F0A4</name></SSID></SSIDConfig>
<connectionType>ESS</connectionType><connectionMode>manual</connectionMode>
<MSM><security><authEncryption><authentication>open</authentication><encryption>none</encryption><useOneX>false</useOneX></authEncryption></security></MSM>
</WLANProfile>''', encoding='utf-8')
results = {}
try:
    assert wlan('add', 'profile', 'filename=' + str(profile_path), 'user=current').returncode == 0
    assert wlan('connect', 'name=' + temporary).returncode == 0
    for _ in range(15):
        if connected_profile() == temporary:
            break
        time.sleep(1)
    assert connected_profile() == temporary, 'AP association failed'
    time.sleep(2)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp:
        udp.settimeout(5)
        udp.sendto(question(host), ('192.168.4.1', 53))
        results['ap_dns'] = addresses(udp.recv(2048))
        assert '192.168.4.1' in results['ap_dns']
    results['numeric_health'] = get('http://192.168.4.1/health').strip()
    status = json.loads(get('http://192.168.4.1/api/status'))
    assert not status['configured'] and status['canonicalHost'] == host
    results['clean_state'] = True
    results['os_resolution'] = sorted({item[4][0] for item in socket.getaddrinfo(host, 80, socket.AF_INET)})
    assert '192.168.4.1' in results['os_resolution']
    page = get('http://' + host + '/setup')
    assert 'lang="th"' in page and 'ตั้งค่า SmartLock' in page
    results['canonical_setup_thai'] = True
    # Direct multicast query, distinct from the AP's ordinary DNS responder.
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as route:
            route.connect(('192.168.4.1', 53))
            local_ip = route.getsockname()[0]
        assert local_ip.startswith('192.168.4.')
        udp.bind((local_ip, 0))
        udp.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(local_ip))
        udp.settimeout(5)
        udp.sendto(question(host, True), ('224.0.0.251', 5353))
        results['ap_mdns'] = addresses(udp.recv(4096))
        assert '192.168.4.1' in results['ap_mdns']
finally:
    wlan('connect', 'name=' + original)
    for _ in range(15):
        if connected_profile() == original:
            break
        time.sleep(1)
    results['original_wifi_restored'] = connected_profile() == original
    wlan('delete', 'profile', 'name=' + temporary)
    print(json.dumps(results), flush=True)
    Path('.pio/canonical-ap-result.json').write_text(json.dumps(results), encoding='utf-8')
    assert results['original_wifi_restored'], 'Original Wi-Fi restoration requires attention'
