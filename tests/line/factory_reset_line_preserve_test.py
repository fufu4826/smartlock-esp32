"""Source-contract checks for the prototype Factory Reset LINE-preservation policy."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def perform_reset_body() -> str:
    source = read("src/app/FactoryResetController.cpp")
    match = re.search(r"Result FactoryResetController::performReset\(\) \{(.*?)\n\}", source, re.S)
    assert match, "performReset not found"
    return match.group(1)


def main() -> int:
    body = perform_reset_body()
    cleared = re.findall(r'clearNamespace\("([^"]+)"\)', body)
    expected = ["sl-config", "sl-net", "sl-install", "sl-pin", "sl-generation", "sl-ota",
                "sl-sta", "sl-cloud", "sl-audit", "p2-test"]
    assert cleared == expected, cleared
    assert "sl-line" not in cleared and "lock-touch-v2" not in cleared
    # Identity/auth SD tree and SDK Wi-Fi credentials are still erased.
    assert 'eraseSmartLockTree("/smartlock", 0)' in body and "WiFi.eraseAP()" in body
    # Runtime-only LINE stop; never the persistent disconnect.
    assert "LineNotifications::suspendForReset()" in body
    assert "LineNotifications::disconnect()" not in body
    # Lock safety ordering preserved: lock first, verify GPIO22 locked before erasing.
    assert body.index("lock_.lock()") < body.index("suspendForReset") < body.index("kLockedLevel") < body.index("eraseAP")

    main_cpp = read("src/main.cpp")
    fresh = re.search(r"const bool noPriorNvs =(.*?);", main_cpp, re.S)
    assert fresh, "noPriorNvs missing"
    for key in ('"sl-install", "id"', '"sl-pin", "initial"', '"sl-pin", "fail"', '"sl-net", "ap-pass"',
                '"sl-sta", "cfg-a"', '"sl-sta", "cfg-b"'):
        assert key in fresh.group(1), key
    assert "sl-line" not in fresh.group(1)
    assert "AdminPin::begin(configured, freshInstallation)" in main_cpp
    pin = read("src/security/AdminPin.cpp")
    assert "if(healthy_&&!present_&&!configured&&freshInstallation)" in pin
    assert "if(!present_){healthy_=false;return false;}" in pin

    line = read("src/notifications/LineNotifications.cpp")
    suspend = re.search(r"Result suspendForReset\(\) \{(.*?)\n\}", line, re.S)
    assert suspend and "Preferences" not in suspend.group(1) and "writeConfig" not in suspend.group(1)
    for fn in ("Result save(", "Result rename(", "Result disconnect() {"):
        start = line.index(fn)
        assert "if(gSuspended)return Result::StorageError;" in line[start:start + 200], fn

    # Broadcast delivery: preserved config needs only token + enabled; no recipient.
    assert 'kDeliveryPath = "/v2/bot/message/broadcast"' in line and "message/push" not in line
    assert "(!c.enabled||validToken(c.token))" in line
    assert "Result save(const char* label,const char* token,const char* publicBasicId)" in line

    # Add Friend is only a LINE-hosted link built from the preserved Basic ID.
    assets = read("src/web/WebAssets.h")
    assert "addFriend.href='https://line.me/R/ti/p/'+encodeURIComponent(data.publicBasicId)" in assets
    assert not re.search(r"lineAddFriend'\)\.addEventListener", assets)
    web = read("src/network/WebServerManager.cpp")
    assert "friend" not in " ".join(re.findall(r'server_\.on\("([^"]+)"', web)).lower()
    print("Factory Reset LINE preservation contract: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
