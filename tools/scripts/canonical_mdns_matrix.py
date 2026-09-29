#!/usr/bin/env python3
"""Bounded LAN-only mDNS query/reply matrix for the canonical IPv4 A record."""
from __future__ import annotations

import datetime as dt
import argparse
import importlib.util
import json
import socket
import struct
import time
from pathlib import Path


HOST = "smartlock-04225a0ff0a4.local"
BOARD = "192.168.1.179"
PC = "192.168.1.145"
GROUP = "224.0.0.251"
PORT = 5353
ROUNDS = 2
TIMEOUT = 4.0


def helpers():
    # Reuse the project's exact-FQDN DNS encoder/parser. Import is side-effect free.
    path = Path(__file__).with_name("canonical_layer_probe.py")
    spec = importlib.util.spec_from_file_location("canonical_layer_probe", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("probe helper unavailable")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def assigned_pc_ip() -> bool:
    try:
        # Bound to the requested host address; this read-only check detects stale
        # assumptions without enumerating other interfaces or network profiles.
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as route:
            route.bind((PC, 0))
        return True
    except OSError:
        return False


def one_probe(parser, encoder, source_mode: str, destination_mode: str,
              qu: bool) -> dict:
    result = {
        "status": "not_started", "bind": "not_attempted", "bind_error": None,
        "join": "not_attempted", "join_error": None, "source_port": None,
        "multicast_ttl": None, "qd": None, "rcode": None,
        "peer_port": None, "elapsed_ms": None, "board_answer_a": None,
    }
    start = time.monotonic()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if source_mode == "5353":
            sock.bind(("0.0.0.0", PORT))
        else:
            sock.bind((PC, 0))
        source_port = sock.getsockname()[1]
        result["source_port"] = source_port
        if source_mode == "5353" and source_port != PORT:
            result.update(status="bind_failed", bind="failed", bind_error="unexpected_source_port")
            return result
        result["bind"] = "ok"
        if destination_mode == "multicast" or source_mode == "5353":
            membership = socket.inet_aton(GROUP) + socket.inet_aton(PC)
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, membership)
            result["join"] = "ok"
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(PC))
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 255)
            result["multicast_ttl"] = sock.getsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL)
            if result["multicast_ttl"] != 255:
                result.update(status="socket_setup_failed", join_error="ttl_not_255")
                return result
        else:
            result["join"] = "not_required"

        destination = BOARD if destination_mode == "unicast" else GROUP
        query = encoder(HOST, 1, 0, qu=qu, rd=False)
        sock.sendto(query, (destination, PORT))
        result["status"] = "no_response"
        deadline = time.monotonic() + TIMEOUT
        while time.monotonic() < deadline:
            sock.settimeout(max(0.05, deadline - time.monotonic()))
            try:
                packet, peer = sock.recvfrom(4096)
            except socket.timeout:
                break
            # Ignore all unrelated traffic. Evidence contains only the board's
            # response carrying the requested canonical owner A record.
            if peer[0] != BOARD:
                continue
            try:
                message = parser(packet)
            except Exception:
                continue
            if not message["response"] or message["id"] != 0:
                continue
            records = [r for r in message["records"]
                       if r["name"] == HOST and r["type"] == "A" and r.get("A")]
            if not records:
                continue
            result.update(status="response", qd=len(message["questions"]),
                          rcode=message["rcode"], peer_port=peer[1],
                          board_answer_a=sorted({r["A"] for r in records}))
            break
    except OSError as exc:
        # Keep each matrix cell independent and retain only the exception class.
        if result["bind"] == "not_attempted":
            result.update(status="bind_failed", bind="failed", bind_error=type(exc).__name__)
        elif result["join"] in ("not_attempted", "not_required"):
            result.update(status="socket_setup_failed", join="failed", join_error=type(exc).__name__)
        else:
            result.update(status="send_or_receive_error", error=type(exc).__name__)
    finally:
        result["elapsed_ms"] = round((time.monotonic() - start) * 1000)
        sock.close()
    return result


def main() -> int:
    args = argparse.ArgumentParser()
    args.add_argument("--output", default="mdns-matrix.json")
    parsed = args.parse_args()
    require = helpers()
    if not assigned_pc_ip():
        raise SystemExit("Required LAN interface address is unavailable; no probes sent.")
    cells = []
    for source_mode in ("ephemeral", "5353"):
        for destination_mode in ("unicast", "multicast"):
            for qu in (False, True):
                rounds = [one_probe(require.parse_dns, require.dns_query,
                                    source_mode, destination_mode, qu)
                          for _ in range(ROUNDS)]
                cells.append({"source_mode": source_mode,
                              "destination": BOARD if destination_mode == "unicast" else GROUP,
                              "destination_mode": destination_mode,
                              "qu": qu, "rounds": rounds})
    evidence = {
        "generated_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "scope": "read-only exact-FQDN IPv4 mDNS A query matrix; no firewall/router/network changes, serial access, or device writes",
        "target": {"canonical_host": HOST, "board_ip": BOARD, "pc_ip": PC,
                   "rounds_per_cell": ROUNDS, "timeout_seconds": TIMEOUT,
                   "multicast_ttl": 255},
        "interpretation": "Only QR responses from the board containing the exact canonical owner A record are retained. Results distinguish query source port, destination, and QU/QM behavior; they do not prove a particular NIC or router forwarding path.",
        "matrix": cells,
    }
    output = Path(parsed.output)
    if not output.is_absolute():
        output = (Path(__file__).resolve().parents[1] /
                  "docs/phase_reports/evidence/canonical_recurrence" / output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    summary = [{"source": c["source_mode"], "destination": c["destination_mode"],
                "qu": c["qu"], "statuses": [r["status"] for r in c["rounds"]],
                "answers": [r["board_answer_a"] for r in c["rounds"]]}
               for c in cells]
    print(json.dumps({"evidence": str(output), "pc_ip_present": True,
                      "matrix": summary}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
