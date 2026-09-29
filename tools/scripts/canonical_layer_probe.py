#!/usr/bin/env python3
"""Read-only, bounded SmartLock canonical-origin/DNS layer probe."""
from __future__ import annotations
import datetime as dt
import argparse
import json
import random
import socket
import struct
import subprocess
import time
import urllib.error
import urllib.request
from pathlib import Path

HOST = "smartlock-04225a0ff0a4.local"
CANONICAL = HOST.lower().rstrip(".")
BOARD = "192.168.1.179"
PC = "192.168.1.145"
ROUNDS = 3
TIMEOUT = 4.0
MDNS_GROUP = "224.0.0.251"
TYPE_NAMES = {1: "A", 5: "CNAME", 12: "PTR", 16: "TXT", 28: "AAAA", 33: "SRV"}


def dns_name(packet: bytes, offset: int):
    labels, pos, end, seen = [], offset, None, set()
    while True:
        if pos >= len(packet):
            raise ValueError("truncated DNS name")
        n = packet[pos]
        if n & 0xC0 == 0xC0:
            if pos + 1 >= len(packet): raise ValueError("truncated DNS pointer")
            ptr = ((n & 0x3F) << 8) | packet[pos + 1]
            if ptr in seen: raise ValueError("DNS pointer loop")
            seen.add(ptr)
            if end is None: end = pos + 2
            pos = ptr
            continue
        if n & 0xC0: raise ValueError("invalid DNS label")
        pos += 1
        if n == 0: return ".".join(labels).lower(), (end if end is not None else pos)
        if n > 63 or pos + n > len(packet): raise ValueError("bad DNS label length")
        labels.append(packet[pos:pos+n].decode("ascii", "replace"))
        pos += n


def dns_query(name: str, qtype: int, ident: int = 0, qu: bool = False, rd: bool = False):
    qname = b"".join(bytes((len(x),)) + x.encode("ascii") for x in name.rstrip(".").split(".")) + b"\0"
    flags = 0x0100 if rd else 0
    return struct.pack("!HHHHHH", ident, flags, 1, 0, 0, 0) + qname + struct.pack("!HH", qtype, 0x8001 if qu else 1)


def parse_dns(data: bytes):
    if len(data) < 12: raise ValueError("short DNS message")
    ident, flags, qd, an, ns, ar = struct.unpack("!HHHHHH", data[:12])
    pos, questions, records = 12, [], []
    for _ in range(min(qd, 16)):
        name, pos = dns_name(data, pos)
        if pos + 4 > len(data): raise ValueError("truncated question")
        typ, cls = struct.unpack("!HH", data[pos:pos+4]); pos += 4
        questions.append({"name": name, "type": TYPE_NAMES.get(typ, str(typ))})
    for _ in range(min(an + ns + ar, 64)):
        name, pos = dns_name(data, pos)
        if pos + 10 > len(data): raise ValueError("truncated record")
        typ, cls, ttl, size = struct.unpack("!HHIH", data[pos:pos+10]); pos += 10
        if pos + size > len(data): raise ValueError("truncated RDATA")
        value = None
        if typ == 1 and size == 4:
            value = socket.inet_ntoa(data[pos:pos+4])
        records.append({"name": name, "type": TYPE_NAMES.get(typ, str(typ)), **({"A": value} if value else {})})
        pos += size
    return {"id": ident, "rcode": flags & 15, "response": bool(flags & 0x8000), "questions": questions, "records": records}


def capture_dns(sock, expected_name, expected_id, expected_peer, window=TIMEOUT):
    deadline = time.monotonic() + window
    found = []
    while time.monotonic() < deadline:
        sock.settimeout(max(0.05, deadline - time.monotonic()))
        try: packet, peer = sock.recvfrom(4096)
        except socket.timeout: break
        if expected_peer and peer[0] != expected_peer:
            continue
        try: parsed = parse_dns(packet)
        except Exception: continue
        # Admit only actual responses for the exact canonical question/owner.
        if not parsed["response"] or parsed["id"] != expected_id:
            continue
        questions = [q for q in parsed["questions"] if q["name"] == expected_name]
        records = [r for r in parsed["records"] if r["name"] == expected_name]
        if questions or records:
            found.append({"peer_ip": peer[0], "rcode": parsed["rcode"], "questions": questions, "records": records})
            if len(found) >= 3: break
    return found


def result(fn):
    start = time.monotonic()
    try:
        data = fn()
        status = "no_response" if data.get("result") == "no_response" else "ok"
        return {"status": status, **data, "elapsed_ms": round((time.monotonic()-start)*1000)}
    except Exception as exc: return {"status": "error", "error": type(exc).__name__, "detail": str(exc)[:220], "elapsed_ms": round((time.monotonic()-start)*1000)}


def http_probe(url):
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    req = urllib.request.Request(url, headers={"Connection": "close"})
    try:
        with opener.open(req, timeout=TIMEOUT) as resp:
            body = resp.read(256)
            return {"http_status": resp.status, "body_prefix": body.decode("utf-8", "replace")[:80]}
    except urllib.error.HTTPError as exc:
        return {"http_status": exc.code, "body_prefix": exc.read(256).decode("utf-8", "replace")[:80]}


def dns_servers(pc_ip):
    # Resolve only the interface owning PC, and retain only its IPv4 DNS addresses.
    ps = f"$i=(Get-NetIPAddress -AddressFamily IPv4 -IPAddress '{pc_ip}' -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty InterfaceIndex); if ($i) {{ (Get-DnsClientServerAddress -AddressFamily IPv4 -InterfaceIndex $i -ErrorAction SilentlyContinue | ForEach-Object {{ $_.ServerAddresses }}) | ConvertTo-Json -Compress }}"
    try:
        raw = subprocess.check_output(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", ps], timeout=4, stderr=subprocess.DEVNULL, text=True).strip()
        obj = json.loads(raw) if raw else []
        if isinstance(obj, str): obj = [obj]
        vals = []
        for v in obj if isinstance(obj, list) else []:
            try:
                ip = socket.inet_aton(v)
                if v not in vals and not v.startswith("127."): vals.append(v)
            except (OSError, TypeError): pass
        return vals
    except Exception:
        return []


def udp_dns_case(target, port, qtype=1, qu=False, local_ip=None, joined=False):
    ident = 0 if port == 5353 else random.randrange(1, 65536)
    query = dns_query(HOST, qtype, ident, qu, rd=(port == 53))
    family = socket.AF_INET
    sock = socket.socket(family, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    try:
        if joined:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            try: sock.bind(("", 5353))
            except OSError as exc: return {"bind": "failed", "bind_error": type(exc).__name__, "responses": []}
            mreq = socket.inet_aton(MDNS_GROUP) + socket.inet_aton(local_ip or "0.0.0.0")
            try: sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
            except OSError as exc: return {"bind": "ok", "join": "failed", "join_error": type(exc).__name__, "responses": []}
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(local_ip))
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 255)
            sock.sendto(query, (target, port))
            responses = capture_dns(sock, CANONICAL, ident, BOARD)
            return {"bind": "ok", "join": "ok", "result": "response" if responses else "no_response", "responses": responses}
        if local_ip:
            sock.bind((local_ip, 0))
            if target == MDNS_GROUP:
                sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(local_ip))
                sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 255)
        sock.sendto(query, (target, port))
        expected_peer = BOARD if target == MDNS_GROUP else target
        responses = capture_dns(sock, CANONICAL, ident, expected_peer)
        return {"result": "response" if responses else "no_response", "responses": responses}
    finally:
        sock.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pc-ip", default=PC)
    parser.add_argument("--rounds", type=int, default=ROUNDS)
    parser.add_argument("--output", default="docs/phase_reports/evidence/canonical_recurrence/lan-before.json")
    parser.add_argument("--band", choices=("2.4GHz", "5GHz", "unknown"), default="unknown")
    args = parser.parse_args()
    pc_ip = args.pc_ip
    rounds = max(1, min(ROUNDS, args.rounds))
    dns = dns_servers(pc_ip)
    local_present = False
    try:
        # Check assigned interface addresses without hostname resolution or profile/SSID enumeration.
        output = subprocess.check_output(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", f"(Get-NetIPAddress -AddressFamily IPv4 -IPAddress '{pc_ip}' -ErrorAction SilentlyContinue | Measure-Object).Count"], timeout=4, stderr=subprocess.DEVNULL, text=True).strip()
        local_present = output == "1"
    except Exception: pass
    bind_ip = pc_ip
    cases = []
    for rnd in range(1, rounds + 1):
        row = {"round": rnd, "cases": {}}
        row["cases"]["os_getaddrinfo"] = result(lambda: {"addresses": sorted({a[4][0] for a in socket.getaddrinfo(HOST, 80, socket.AF_INET, socket.SOCK_STREAM)})})
        row["cases"]["numeric_ip_health"] = result(lambda: http_probe(f"http://{BOARD}/health"))
        row["cases"]["canonical_url_health"] = result(lambda: http_probe(f"http://{HOST}/health"))
        row["cases"]["mdns_unicast_udp5353_A"] = result(lambda: udp_dns_case(BOARD, 5353, local_ip=bind_ip))
        row["cases"]["mdns_multicast_QU_A"] = result(lambda: udp_dns_case(MDNS_GROUP, 5353, qu=True, local_ip=bind_ip))
        row["cases"]["mdns_multicast_QM_A_joined_udp5353"] = result(lambda: udp_dns_case(MDNS_GROUP, 5353, local_ip=bind_ip, joined=True))
        row["cases"]["dns_udp53_board_A"] = result(lambda: udp_dns_case(BOARD, 53, local_ip=bind_ip))
        row["cases"]["dns_udp53_lan_A"] = [{"server_ip": server, **result(lambda server=server: udp_dns_case(server, 53, local_ip=bind_ip))} for server in dns]
        cases.append(row)
    doc = {
        "generated_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "scope": "read-only LAN DNS/mDNS/HTTP probes; no credentials, auth, router, Wi-Fi, host files, or device settings touched",
        "target": {"canonical_host": HOST, "board_ip_for_diagnostics_only": BOARD, "expected_pc_ip": pc_ip, "pc_ip_present": local_present, "pc_link_band": args.band, "rounds": rounds, "timeout_seconds": TIMEOUT},
        "dns_servers_ipv4": dns,
        "interpretation": "numeric-IP HTTP is diagnostic only; canonical_url_health requires OS resolution and HTTP at the canonical origin; all DNS questions use the exact .local FQDN; DNS output admits only QR responses for the canonical name and reports owner/type/A/rcode",
        "rounds": cases,
    }
    out = Path(args.output)
    if not out.is_absolute(): out = Path(__file__).resolve().parents[1] / out
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"evidence": str(out), "pc_ip_present": local_present, "dns_server_count": len(dns), "rounds": rounds, "case_names": list(cases[0]["cases"].keys())}, indent=2))

if __name__ == "__main__": main()
