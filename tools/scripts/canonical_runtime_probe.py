"""Read-only canonical mDNS + HTTP check with explicit Windows resolver evidence."""
import ast
import json
from pathlib import Path
import socket
import sys
import urllib.request

host = sys.argv[1] if len(sys.argv) > 1 else 'smartlock-04225a0ff0a4.local'
route_ip = sys.argv[2] if len(sys.argv) > 2 else '192.168.1.179'
source = ast.parse(Path(__file__).with_name('canonical_lan_probe.py').read_text())
helpers = ast.Module(body=[n for n in source.body if isinstance(n, ast.FunctionDef)
                         and n.name in ('question', 'skip_name', 'addresses')], type_ignores=[])
import struct
exec(compile(helpers, '<DNS helpers>', 'exec'))
try:
    os_addresses = sorted({a[4][0] for a in socket.getaddrinfo(host, 80, socket.AF_INET)})
except socket.gaierror:
    os_addresses = []
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as route:
    route.connect((route_ip, 53))
    local_ip = route.getsockname()[0]
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp:
    udp.bind((local_ip, 0))
    udp.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(local_ip))
    udp.settimeout(8)
    udp.sendto(question(host, True), ('224.0.0.251', 5353))
    advertised = addresses(udp.recv(4096))
assert route_ip in advertised, 'Canonical mDNS must advertise expected board IP'
opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
for path in ('/health', '/api/status'):
    request = urllib.request.Request('http://' + route_ip + path, headers={'Host': host})
    with opener.open(request, timeout=8) as response:
        data = response.read().decode('utf-8')
    if path == '/health':
        assert data == 'ok\n'
    else:
        status = json.loads(data)
        assert status['configured'] and status['canonicalHost'] == host
print(json.dumps({'canonical_mdns': advertised, 'canonical_host_http': 'PASS',
                  'windows_os_resolution': os_addresses or 'DEFERRED: resolver failure'}, indent=2))
