"""Synthetic mobile browser checks for the production Owner LINE page."""

import json
import re
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

from playwright.sync_api import sync_playwright


ROOT = Path(__file__).resolve().parents[2]
# Evidence goes to a temp directory; the source checkout stays clean.
EVIDENCE = Path(tempfile.gettempdir()) / "smartlock-management-browser" / "management-browser.json"
EVIDENCE.parent.mkdir(parents=True, exist_ok=True)
SOURCE = (ROOT / "src" / "web" / "WebAssets.h").read_text(encoding="utf-8")
MATCH = re.search(r'static const char kManage\[\] PROGMEM = R"HTML\((.*?)\)HTML";', SOURCE, re.DOTALL)
if not MATCH:
    raise RuntimeError("production Management HTML was not found")
MANAGEMENT_HTML = MATCH.group(1)

SESSION = "synthetic-session"
STATE = {}
REQUESTS = []


def reset_state(role="Owner", configured=True):
    STATE.clear()
    STATE.update(
        role=role, configured=configured, enabled=configured, staConnected=True,
        quotaKnown=True, queued=0, sentThisRun=0, failedThisRun=0,
        quotaLimit=300, quotaUsed=3, deviceLabel="SmartLock fixture",
        state="ready" if configured else "disabled", testState=0,
        configVersion=2, publicBasicId="@synthetic" if configured else "",
        testPolls=0, armed=False, unlockSeconds=5, pin="2468", pinFailures=0,
    )
    REQUESTS.clear()


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_args):
        pass

    def send_json(self, code, value):
        body = json.dumps(value, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        body = MANAGEMENT_HTML.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        fields = {k: v[-1] for k, v in parse_qs(self.rfile.read(length).decode(), keep_blank_values=True).items()}
        path = urlsplit(self.path).path
        REQUESTS.append({"path": path, "fields": fields})
        if path == "/api/manage/login":
            self.send_json(200, {"ok": True, "token": SESSION})
            return
        if fields.get("token") != SESSION:
            self.send_json(403, {"error": "forbidden"})
            return
        if path == "/api/manage/state":
            self.send_json(200, {"actorRole": STATE["role"], "lanEnrollment": True, "identities": []})
        elif path == "/api/manage/summary":
            self.send_json(200, {"uptimeMs": 1, "freeHeap": 1000, "minFreeHeap": 900, "databaseHealthy": True, "firmware": "fixture", "canonicalHost": "fixture"})
        elif path == "/api/line/maintenance/arm":
            assert set(fields) == {"token", "pin"}
            assert fields["pin"] == "1234"
            self.send_json(200, {"ok": True, "nonce": "a" * 32, "expiresIn": 120})
        elif path == "/api/line/maintenance/cancel":
            assert set(fields) == {"token"}
            self.send_json(200, {"ok": True})
        elif path == "/api/line/status":
            if STATE["testState"] == 1:
                STATE["testPolls"] += 1
                if STATE["testPolls"] >= 3:
                    STATE["testState"] = 2
            self.send_json(200, {"ok": True, **STATE})
        elif path == "/api/line/label":
            assert set(fields) == {"token", "label"}
            STATE["deviceLabel"] = fields["label"]
            self.send_json(200, {"ok": True})
        elif path == "/api/line/test":
            assert set(fields) == {"token"}
            STATE.update(queued=STATE["queued"] + 1, testState=1, testPolls=0)
            self.send_json(200, {"ok": True})
        elif path == "/api/manage/unlock-duration":
            if STATE["role"] != "Owner":
                self.send_json(403, {"error": "owner_required"})
                return
            if "seconds" in fields:
                assert set(fields) == {"token", "seconds"}
                seconds = int(fields["seconds"]) if fields["seconds"].isdigit() else 0
                if not 1 <= seconds <= 60:
                    self.send_json(400, {"error": "invalid_duration"})
                    return
                STATE["unlockSeconds"] = seconds
            else:
                assert set(fields) == {"token"}
            self.send_json(200, {"unlockSeconds": STATE["unlockSeconds"], "min": 1, "max": 60, "saved": "seconds" in fields})
        elif path == "/api/system/admin-pin":
            assert set(fields) == {"token", "current", "next", "confirm"}
            if STATE["role"] != "Owner":
                self.send_json(403, {"error": "owner_required"})
            elif STATE["pinFailures"] >= 3:
                self.send_json(429, {"error": "pin_locked"})
            elif fields["current"] != STATE["pin"]:
                STATE["pinFailures"] += 1
                locked = STATE["pinFailures"] >= 3
                self.send_json(429 if locked else 403, {"error": "pin_locked" if locked else "current_pin_rejected"})
            else:
                STATE.update(pin=fields["next"], pinFailures=0)
                self.send_json(200, {"changed": True})
        elif path == "/api/line/disconnect":
            assert set(fields) == {"token"}
            STATE.update(configured=False, enabled=False, queued=0, state="disabled", publicBasicId="")
            self.send_json(200, {"ok": True})
        else:
            self.send_json(404, {"error": "not_found"})


def run_checks():
    reset_state()
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    url = f"http://127.0.0.1:{server.server_port}/manage?session=fixture#system"
    result = {"viewport": {"width": 390, "height": 844}, "management": "production kManage", "syntheticOnly": True, "checks": []}
    stage = "launch"
    try:
        with sync_playwright() as playwright:
            browser = playwright.chromium.launch(headless=True)

            def new_page(role, configured=True):
                reset_state(role, configured)
                context = browser.new_context(viewport={"width": 390, "height": 844})
                context.add_init_script(
                    "localStorage.setItem('smartlock.setup.v1',"
                    "JSON.stringify({state:'active',credential:'" + "a" * 64 + "'}));"
                )
                page = context.new_page()
                errors = []
                page.on("pageerror", lambda error: errors.append(str(error)))
                page.goto(url, wait_until="domcontentloaded")
                page.wait_for_function("!document.getElementById('app').hidden", timeout=8000)
                return context, page, errors

            context, page, errors = new_page("Owner")
            context.route("https://line.me/**", lambda route: route.abort())
            no_overflow = "document.documentElement.scrollWidth<=window.innerWidth"
            stage = "Owner navigation"
            labels = [t.strip() for t in page.locator(".manageNav a:visible").all_inner_texts()]
            assert labels == ["ภาพรวม", "อุปกรณ์", "เครือข่าย", "LINE", "ระยะเวลาปลดล็อก", "เปลี่ยนรหัส Admin PIN", "ระบบ"], labels
            assert page.evaluate(no_overflow)
            stage = "LINE page with Add Friend"
            page.locator('#lineNav').click()
            page.wait_for_function("!document.getElementById('linePanel').hidden && !document.getElementById('lineAddFriend').hidden")
            assert page.locator('#lineReady').inner_text() == 'พร้อมใช้งาน'
            assert page.locator('#lineEnabled').inner_text() == 'เปิด'
            friend = page.locator('#lineAddFriend')
            assert friend.is_visible() and friend.inner_text().strip() == 'เพิ่มเพื่อน LINE'
            assert friend.get_attribute('href') == 'https://line.me/R/ti/p/%40synthetic'
            assert friend.evaluate("e=>getComputedStyle(e).backgroundColor") == 'rgb(6, 199, 85)'
            assert friend.bounding_box()["height"] >= 44
            before = len(REQUESTS)
            with context.expect_page() as popup:
                friend.click()
            popup.value.close()
            assert len(REQUESTS) == before, "Add Friend must not call any SmartLock API"
            assert page.evaluate(no_overflow)
            page.screenshot(path=str(EVIDENCE.parent / 'mobile-line-page.png'))
            result['checks'].append('Owner LINE page shows readiness, enabled state and a green Add Friend link built only from the public Basic ID; clicking it sends no SmartLock request')

            stage = "unlock duration page"
            page.locator('#unlockNav').click()
            page.wait_for_function("document.getElementById('unlockCurrent').textContent==='5'")
            assert page.locator('#unlockSeconds').get_attribute('inputmode') == 'numeric'
            assert page.locator('#unlockSeconds').input_value() == '5'
            for bad in ('0', '61'):
                page.evaluate("v=>{document.getElementById('unlockSeconds').value=v;document.getElementById('unlockForm').dispatchEvent(new Event('submit',{cancelable:true}))}", bad)
                assert '1–60' in page.locator('#unlockResult').inner_text()
            assert not any(r["path"] == "/api/manage/unlock-duration" and "seconds" in r["fields"] for r in REQUESTS)
            page.locator('#unlockSeconds').fill('12')
            page.locator('#unlockForm button').click()
            page.wait_for_function("document.getElementById('unlockResult').className==='ok'")
            assert page.locator('#unlockCurrent').inner_text() == '12' and STATE["unlockSeconds"] == 12
            saved = [r for r in REQUESTS if r["path"] == "/api/manage/unlock-duration"][-1]
            assert saved["fields"] == {"token": SESSION, "seconds": "12"}
            assert not any(r["path"].startswith("/api/access") for r in REQUESTS)
            assert page.evaluate(no_overflow)
            page.screenshot(path=str(EVIDENCE.parent / 'mobile-unlock-page.png'))
            result['checks'].append('Owner reads and saves unlock duration in seconds (1-60); invalid values are rejected before any request; saving never calls an unlock API')

            stage = "Admin PIN change page"
            page.locator('#adminPinNav').click()
            page.wait_for_function("!document.getElementById('adminPinPanel').hidden")
            for name in ("current", "next", "confirm"):
                field = page.locator(f'#adminPinForm input[name={name}]')
                assert field.get_attribute('type') == 'password' and field.get_attribute('inputmode') == 'numeric'

            def change_pin(current, new, confirm):
                page.locator('#adminPinForm input[name=current]').fill(current)
                page.locator('#adminPinForm input[name=next]').fill(new)
                page.locator('#adminPinForm input[name=confirm]').fill(confirm)
                page.evaluate("document.getElementById('adminPinResult').textContent='';document.getElementById('adminPinForm').dispatchEvent(new Event('submit',{cancelable:true}))")
                page.wait_for_function("!['','กำลังตรวจสอบรหัส…'].includes(document.getElementById('adminPinResult').textContent)")
                text = page.locator('#adminPinResult').inner_text()
                values = page.evaluate("[...document.querySelectorAll('#adminPinForm input')].map(e=>e.value)")
                assert values == ["", "", ""], values
                return text

            posts = lambda: [r for r in REQUESTS if r["path"] == "/api/system/admin-pin"]
            assert change_pin("2468", "1357", "1358") == "รหัสใหม่ไม่ตรงกัน" and not posts()
            assert change_pin("2468", "13a7", "13a7") == "รหัสใหม่ต้องเป็นตัวเลข 4 หลัก" and not posts()
            assert change_pin("1111", "1357", "1357") == "รหัส Admin PIN ปัจจุบันไม่ถูกต้อง" and len(posts()) == 1
            assert change_pin("2468", "1357", "1357") == "เปลี่ยนรหัส Admin PIN สำเร็จ" and STATE["pin"] == "1357"
            assert change_pin("2468", "9753", "9753") == "รหัส Admin PIN ปัจจุบันไม่ถูกต้อง"
            assert change_pin("0000", "9753", "9753") == "รหัส Admin PIN ปัจจุบันไม่ถูกต้อง"
            assert "60 วินาที" in change_pin("0001", "9753", "9753")
            assert STATE["pin"] == "1357"
            for r in REQUESTS:
                assert "?" not in r["path"]
            storage = page.evaluate("JSON.stringify([Object.entries(localStorage),Object.entries(sessionStorage),document.cookie])")
            for secret in ("2468", "1357", "9753"):
                assert secret not in storage and secret not in page.url
            assert page.evaluate(no_overflow)
            page.screenshot(path=str(EVIDENCE.parent / 'mobile-admin-pin-page.png'))
            result['checks'].append('Owner changes Admin PIN with current PIN; mismatch/format errors never reach the server; wrong current PIN and lockout show Thai errors; fields always cleared; no PIN in URL or browser storage')

            stage = "Owner top-level LINE navigation"
            page.locator('#lineNav').click()
            page.wait_for_function("!document.getElementById('linePanel').hidden && !document.getElementById('lineAddFriend').hidden")
            assert page.locator("#lineAddFriend").get_attribute("href") == "https://line.me/R/ti/p/%40synthetic"
            assert page.locator("#lineAddFriend").get_attribute("target") == "_blank"
            assert page.locator("#linePanel input").count() == 2
            assert page.locator("#linePanel input[type=password]").count() == 1
            assert page.locator("#lineMaintenanceForm input").count() == 1
            assert "lineToken" not in SOURCE and "userId" not in SOURCE
            assert page.locator("#lineMaintenanceForm").is_hidden()
            assert "/api/line/save" not in SOURCE
            stage = "label-only rename"
            page.locator("#lineLabel").fill("SmartLock Mobile")
            page.locator("#lineLabelForm button").click()
            page.wait_for_function("document.getElementById('lineSavedLabel').textContent==='SmartLock Mobile'")
            rename = [r for r in REQUESTS if r["path"] == "/api/line/label"][-1]
            assert set(rename["fields"]) == {"token", "label"}
            assert "userId" not in json.dumps(REQUESTS) and "lineToken" not in json.dumps(REQUESTS)
            assert "synthetic" not in json.dumps(
                page.evaluate("Object.fromEntries(Object.keys(localStorage).map(k=>[k,localStorage.getItem(k)]))")
            )
            result["checks"].append("LINE page keeps label rename, no token/recipient fields and no secrets in browser storage")

            stage = "test state semantics"
            page.locator("#lineTest").click()
            page.wait_for_function("!document.getElementById('lineTest').disabled && document.getElementById('lineResult').className==='ok'", timeout=8000)
            assert "รับคำขอแล้ว" in page.locator("#lineResult").inner_text()
            assert "ส่งถึงแล้ว" not in page.locator("#lineResult").inner_text()
            assert set([r for r in REQUESTS if r["path"] == "/api/line/test"][-1]["fields"]) == {"token"}
            result["checks"].append("API test acceptance is distinguished from delivery; no recipient or token is submitted by the page")

            stage = "Owner PIN gated LINE maintenance"
            page.locator("#lineArm").click()
            assert page.locator("#lineMaintenancePin").get_attribute("type") == "password"
            page.locator("#lineMaintenancePin").fill("1234")
            page.locator("#lineMaintenanceForm button[type=submit]").click()
            page.wait_for_function("!document.getElementById('lineMaintenanceHelp').hidden")
            armed = [r for r in REQUESTS if r["path"] == "/api/line/maintenance/arm"][-1]
            assert set(armed["fields"]) == {"token", "pin"} and armed["fields"]["pin"] == "1234"
            assert not any(r["path"] == "/api/line/maintenance/commit" for r in REQUESTS)
            assert "120" in page.locator("#lineMaintenanceHelp").inner_text()
            assert page.locator("#lineMaintenanceForm").is_hidden()
            result["checks"].append("Owner supplies only masked PIN; browser arms a 120-second developer-only provisioning window without LINE credentials")
            page.locator("#lineArm").click()
            page.locator("#lineCancelMaintenance").click()
            page.wait_for_function("document.getElementById('lineMaintenanceHelp').hidden")
            cancel = [r for r in REQUESTS if r["path"] == "/api/line/maintenance/cancel"][-1]
            assert set(cancel["fields"]) == {"token"}
            result["checks"].append("Owner can cancel the provisioning window using the authenticated management session")

            stage = "disconnect explanation"
            page.locator("#lineDisconnect").click()
            assert "บัญชี LINE" in page.locator("#actionDialogText").inner_text()
            page.locator("#actionDialogConfirm").click()
            page.wait_for_function("document.getElementById('lineSetupHelp').hidden===false")
            assert set([r for r in REQUESTS if r["path"] == "/api/line/disconnect"][-1]["fields"]) == {"token"}
            assert not errors, errors
            context.close()

            stage = "unconfigured and Admin visibility"
            context, page, errors = new_page("Owner", configured=False)
            page.goto(url.replace("#system", "#line"), wait_until="domcontentloaded")
            page.wait_for_function("!document.getElementById('linePanel').hidden")
            assert page.locator("#lineSetupHelp").is_visible()
            assert page.locator("#lineAddFriend").is_hidden()
            assert "ยังไม่มีการตั้งค่า LINE" in page.locator("#lineFriendHelp").inner_text()
            assert page.locator("#lineReady").inner_text() == "ยังไม่พร้อม" and page.locator("#lineEnabled").inner_text() == "ปิด"
            context.close()
            context, page, errors = new_page("Admin")
            assert page.locator("#lineSystemLinkRow").is_hidden()
            assert page.locator("#lineNav").is_hidden()
            assert page.locator("#unlockNav").is_hidden() and page.locator("#adminPinNav").is_hidden()
            for owner_page, panel in (("line", "#linePanel"), ("unlock", "#unlockPanel"), ("admin-pin", "#adminPinPanel")):
                page.goto(url.replace("#system", "#" + owner_page), wait_until="domcontentloaded")
                page.wait_for_function("!document.getElementById('app').hidden")
                assert page.locator(panel).is_hidden(), owner_page
            assert not any(r["path"] in ("/api/manage/unlock-duration", "/api/system/admin-pin", "/api/line/status") for r in REQUESTS)
            assert not errors, errors
            context.close()
            result["checks"].append("Unconfigured state hides Add Friend with a Thai explanation; Admin role cannot see or open LINE, unlock-duration or Admin PIN pages")
            browser.close()
        result["result"] = "PASS"
    except Exception as error:
        result.update(result="FAIL", stage=stage, failure=str(error)[:320], observedRequests=[r["path"] for r in REQUESTS])
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)
    return result


if __name__ == "__main__":
    report = run_checks()
    EVIDENCE.parent.mkdir(parents=True, exist_ok=True)
    EVIDENCE.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))
    raise SystemExit(0 if report["result"] == "PASS" else 1)
