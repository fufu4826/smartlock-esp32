"""Non-unlocking LAN API checks. No real Access session or credential is used."""
import urllib.error
import urllib.parse
import urllib.request
import sys

origin = "http://" + (sys.argv[1] if len(sys.argv) > 1 else "192.168.1.179")
with urllib.request.urlopen(origin + "/health", timeout=5) as response:
    assert response.status == 200 and response.read() == b"ok\n"
print("LAN health: PASS")
with urllib.request.urlopen(origin + "/verify-owner", timeout=5) as response:
    page = response.read()
    assert response.status == 200 and b"api/owner/verify" in page
    assert "ยืนยันเจ้าของ" in page.decode("utf-8")
print("LAN Verify Owner page: PASS")
fake = "0" * 64
with urllib.request.urlopen(origin + "/a/" + fake, timeout=5) as response:
    page = response.read()
    assert response.status == 200 and b"api/access/request" in page
print("LAN Access page route: PASS")

def reject(path, fields, expected):
    body = urllib.parse.urlencode(fields).encode()
    request = urllib.request.Request(origin + path, body,
                                     {"Content-Type": "application/x-www-form-urlencoded"})
    try:
        urllib.request.urlopen(request, timeout=5)
        raise AssertionError(f"{path}: unexpectedly accepted")
    except urllib.error.HTTPError as error:
        assert error.code == expected, (path, error.code, expected)

reject("/api/access/request", {"session": fake, "deviceId": "D000001",
                               "credential": fake}, 403)
print("Unknown Access session: DENIED")
reject("/api/access/request", {"session": fake}, 400)
print("Missing credential: DENIED")
reject("/api/owner/verify", {"deviceName": "Preflight"}, 403)
print("Incomplete Owner verification: DENIED")
with urllib.request.urlopen(origin + "/owner-bootstrap?session=" + fake,
                            timeout=5) as response:
    page = response.read()
    assert response.status == 200 and b"api/network/bootstrap/complete" in page
print("LAN Owner bootstrap page route: PASS")
reject("/api/network/bootstrap/create", {"token": fake}, 403)
print("Unauthenticated bootstrap creation: DENIED")
reject("/api/network/bootstrap/complete",
       {"session": fake, "credential": fake, "deviceName": "Preflight"}, 403)
print("Unknown bootstrap session: DENIED")
