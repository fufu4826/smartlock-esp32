"""Read-only LAN probe. Usage: python scripts/phase6a_lan_probe.py 192.168.x.y"""
import json
import sys
import urllib.error
import urllib.request

if len(sys.argv) != 2:
    raise SystemExit("Usage: phase6a_lan_probe.py <LAN IPv4>")
ip = sys.argv[1]
parts = ip.split(".")
if len(parts) != 4 or not all(p.isdigit() and 0 <= int(p) <= 255 for p in parts):
    raise SystemExit("Invalid IPv4 address")
origin = f"http://{ip}"
with urllib.request.urlopen(origin + "/health", timeout=5) as response:
    assert response.status == 200 and response.read() == b"ok\n"
print("LAN /health: PASS")
with urllib.request.urlopen(origin + "/api/status", timeout=5) as response:
    status = json.load(response)
    assert response.status == 200 and status.get("configured") is True
print("LAN /api/status: PASS (configured)")
for path in ("/api/network/status", "/api/network/scan", "/api/network/connect",
             "/api/network/bootstrap/create"):
    body = b"token=" + b"0" * 64
    request = urllib.request.Request(origin + path, body,
                                     {"Content-Type": "application/x-www-form-urlencoded"})
    try:
        urllib.request.urlopen(request, timeout=5)
        raise AssertionError(f"{path}: unauthorized request accepted")
    except urllib.error.HTTPError as error:
        assert error.code in (400, 403), (path, error.code)
    print(f"LAN {path}: unauthorized denied")
