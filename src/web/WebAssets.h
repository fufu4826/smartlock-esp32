#pragma once

#include <Arduino.h>

namespace WebAssets {

// Network page section. It is also embedded in the management SPA so the
// management token stays in memory and is never placed in a URL or storage.
static const char kNetwork[] PROGMEM = R"HTML(<section id="networkPanel" hidden>
<h2>เครือข่าย</h2><p class="muted">ใช้งานปกติผ่าน Wi-Fi บ้าน จุดกระจายสัญญาณจะปิดหลังยืนยันการเข้าถึงผ่าน LAN แล้ว 3 วินาที หาก Wi-Fi บ้านขาดการเชื่อมต่อ 60 วินาที ระบบจะเปิดจุดกระจายสัญญาณกู้คืนที่มีรหัสผ่าน</p>
<div class="networkGrid" aria-live="polite"><div><span class="muted">โหมดปัจจุบัน</span><strong id="networkMode">—</strong></div><div><span class="muted">SSID ของจุดกระจายสัญญาณ</span><strong id="networkApSsid">—</strong></div><div><span class="muted">IP ของจุดกระจายสัญญาณ</span><strong id="networkApIp">—</strong></div><div><span class="muted">สถานะการเชื่อมต่อ</span><strong id="networkStaState">—</strong></div><div><span class="muted">SSID ที่เชื่อมต่อ</span><strong id="networkStaSsid">—</strong></div><div><span class="muted">IP ของอุปกรณ์ / IP ใน LAN</span><strong id="networkStaIp">—</strong></div><div><span class="muted">ความแรงสัญญาณ</span><strong id="networkRssi">—</strong></div></div>
<h2>เครือข่าย Wi-Fi ที่พบ</h2><button id="scanNetworks" type="button">ค้นหา Wi-Fi</button><div id="networkList" class="networkList" aria-live="polite"></div>
<form id="networkConnectForm"><label for="networkPassword">รหัสผ่าน Wi-Fi</label><input id="networkPassword" type="password" autocomplete="new-password" autocapitalize="off"><button id="networkConnect" type="submit">ทดสอบและเชื่อมต่อ</button></form><p id="canonicalLinkRow" hidden><a id="canonicalAnchor">เปิด SmartLock ผ่านชื่อเดิม</a></p><p class="muted">เมื่อเปลี่ยนไปใช้ Wi-Fi บ้าน ให้เปิดชื่อ SmartLock เดิมในเบราว์เซอร์เครื่องเดิม ระบบจะใช้ข้อมูลยืนยันเดิมโดยไม่ต้องส่งต่อสิทธิ์</p><p id="networkResult" role="status" aria-live="polite"></p>
</section>)HTML";

static const char kIndex[] PROGMEM = R"HTML(<!doctype html>
<html lang="th"><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SmartLock</title><style>body{font:16px system-ui,sans-serif;max-width:36rem;margin:3rem auto;padding:0 1rem;color:#17212b}a{color:#075985}</style>
<h1>SmartLock</h1><p>เครือข่ายสำหรับตั้งค่าอุปกรณ์พร้อมใช้งานแล้ว</p>
<p id="canonicalHomeRow" hidden>ใช้ชื่อ SmartLock เดิมเมื่อเปลี่ยนเครือข่าย: <a id="canonicalHomeLink">เปิด SmartLock</a></p><p><a href="/setup">ข้อมูลการตั้งค่า</a> | <a href="/api/status">สถานะอุปกรณ์</a></p>
<script>(()=>{'use strict';fetch('/api/status',{cache:'no-store'}).then((response)=>{if(!response.ok)throw new Error('status');return response.json();}).then((data)=>{if(!data||typeof data.canonicalHost!=='string'||!/^smartlock-[0-9a-f]{12}\.local$/i.test(data.canonicalHost))return;const link=document.getElementById('canonicalHomeLink');link.href='http://'+data.canonicalHost+'/';document.getElementById('canonicalHomeRow').hidden=false;}).catch(()=>{});})();</script></html>)HTML";

static const char kSetup[] PROGMEM = R"HTML(<!doctype html>
<html lang="th">
<head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="color-scheme" content="light"><title>ตั้งค่า SmartLock</title>
<style>
:root{font:16px/1.45 system-ui,-apple-system,"Segoe UI",sans-serif;color:#17212b;background:#f3f6f8}
*{box-sizing:border-box}body{margin:0;padding:1rem;min-height:100vh;display:grid;place-items:center}
main{width:min(100%,34rem);background:#fff;border:1px solid #dbe3e8;border-radius:1rem;padding:clamp(1.1rem,5vw,2rem);box-shadow:0 8px 30px #102a3a12}
h1{font-size:1.55rem;line-height:1.2;margin:0 0 .45rem}p{margin:.45rem 0 1.1rem;color:#455564}
label{display:block;font-weight:650;margin:1rem 0 .35rem}input{font:inherit;width:100%;min-height:2.9rem;padding:.65rem .75rem;border:1px solid #9aaab5;border-radius:.55rem;color:#17212b;background:#fff}
input:focus{outline:3px solid #93c5fd;border-color:#1769aa}input:disabled{background:#eef2f4;color:#53616c}
.hint{display:block;font-size:1rem;font-weight:400;color:#53616c;margin-top:.2rem}
button{font:inherit;font-weight:650;min-height:2.9rem;padding:.65rem 1rem;border:0;border-radius:.55rem;background:#075985;color:#fff;cursor:pointer;width:100%;margin-top:1.25rem}
button:disabled{opacity:.6;cursor:wait}.secondary{background:#e8eef2;color:#17212b;margin-top:.6rem}
#status{margin:1rem 0 0;padding:.75rem;border-radius:.5rem;white-space:pre-wrap}#status:empty{display:none}.error{background:#fff0ed;color:#842b1c}.ok{background:#eaf7ee;color:#17552d}.note{background:#eef6fc;color:#174a70}
[hidden]{display:none!important}
</style>
</head>
<body><main>
<h1>ตั้งค่า SmartLock</h1>
<p id="intro">หลังเชื่อมต่อ Wi-Fi ของ SmartLock แล้ว ให้เปิดหน้านี้ด้วยเบราว์เซอร์ที่ใช้ประจำ กรอกรายละเอียดอุปกรณ์และเจ้าของด้านล่าง แล้วเปิดหน้านี้ค้างไว้จนกว่าจะยืนยันการตั้งค่าเสร็จ</p>
<p id="canonicalOriginHint" class="note" hidden>การตั้งค่าปกติควรเปิดด้วยชื่อ SmartLock หลักในเบราว์เซอร์เครื่องเดิม โดยใช้เซสชันจาก QR นี้: <a id="canonicalSetupLink">เปิดการตั้งค่าผ่านชื่อหลัก</a> ตัวเลข IP ใช้สำหรับตรวจสอบหรือกู้คืนเท่านั้น และอาจเป็นพื้นที่จัดเก็บข้อมูลยืนยันคนละชุด</p>
<section id="noSession" hidden aria-live="polite">
  <p>เริ่มตั้งค่าโดยแตะหรือสแกน QR สำหรับตั้งค่าที่แสดงบนอุปกรณ์ QR นี้จะเปิดหน้านี้พร้อมเซสชันตั้งค่าที่ใช้ได้ครั้งเดียว</p>
  <p><a href="/">กลับไปหน้าหลักของอุปกรณ์</a></p>
</section>
<form id="setupForm" hidden autocomplete="on">
  <label for="ownerName">ชื่อเจ้าของ <span class="hint">1–40 ไบต์ UTF-8</span></label>
  <input id="ownerName" name="ownerName" required autocomplete="name">
  <label for="unlockSeconds">ระยะเวลาปลดล็อก <span class="hint">1–60 วินาที</span></label>
  <input id="unlockSeconds" name="unlockSeconds" type="number" required min="1" max="60" step="1" inputmode="numeric" value="5">
  <label for="adminPin">รหัส ADMIN 4 หลัก</label><input id="adminPin" type="password" inputmode="numeric" pattern="[0-9]{4}" minlength="4" maxlength="4" required autocomplete="new-password"><label for="pinConfirm">ยืนยันรหัส ADMIN</label><input id="pinConfirm" type="password" inputmode="numeric" pattern="[0-9]{4}" maxlength="4" required autocomplete="new-password">
  <button id="submitButton" type="submit">ตั้งค่าอุปกรณ์</button>
<button id="restartButton" class="secondary" type="button" hidden>เริ่มตั้งค่าใหม่ด้วยข้อมูลยืนยันชุดใหม่</button>
</form>
<section id="networkDetails" hidden aria-live="polite">
  <p>ตั้งค่าเสร็จแล้ว เชื่อมต่อเครือข่าย Wi-Fi ของอุปกรณ์อีกครั้งโดยใช้ข้อมูลนี้:</p>
  <p><strong>ชื่อ Wi-Fi (SSID):</strong> <span id="apSsid"></span><br><strong>รหัสผ่าน Wi-Fi:</strong> <span id="apPassword"></span></p>
  <p>หากโทรศัพท์ยังจำเครือข่ายเดิมที่ไม่ใช้รหัสผ่าน ให้ลบเครือข่ายนั้นออกแล้วเชื่อมต่อใหม่ด้วยรหัสผ่านนี้</p>
</section>
<section id="pendingNetworkDetails" hidden aria-live="polite">
  <p>คำขอตั้งค่ายังรอการยืนยัน หาก SmartLock เริ่มจุดกระจายสัญญาณที่ป้องกันด้วยรหัสผ่านหลังรีสตาร์ต ให้เชื่อมต่อเครือข่าย SmartLock เดิมด้วยรหัสผ่านชั่วคราวนี้ การแสดงข้อมูลนี้ยังไม่ยืนยันว่า Owner ลงทะเบียนสำเร็จ</p>
  <p><strong>รหัสผ่านจุดกระจายสัญญาณที่รอใช้:</strong> <span id="pendingApPassword"></span></p>
</section>
<p id="status" role="status" aria-live="polite"></p>
</main>
<script>
(async () => {
  'use strict';
  const SESSION = new URLSearchParams(window.location.search).get('session') || '';
  const form = document.getElementById('setupForm');
  const noSession = document.getElementById('noSession');
  const status = document.getElementById('status');
  const submitButton = document.getElementById('submitButton');
  const restartButton = document.getElementById('restartButton');
  fetch('/api/status', {cache: 'no-store'}).then((response) => { if (!response.ok) throw new Error('status'); return response.json(); }).then((data) => {
    if (!data || typeof data.canonicalHost !== 'string' || !/^smartlock-[0-9a-f]{12}\.local$/i.test(data.canonicalHost) || location.hostname.toLowerCase() === data.canonicalHost.toLowerCase()) return;
    document.getElementById('canonicalSetupLink').href = 'http://' + data.canonicalHost + location.pathname + location.search;
    document.getElementById('canonicalOriginHint').hidden = false;
  }).catch(async () => {});
  const fields = ['ownerName', 'unlockSeconds', 'adminPin', 'pinConfirm'];
  const setupMessages = {ownerName:'กรอกชื่อเจ้าของไม่เกิน 40 ไบต์ UTF-8',unlockSeconds:'กรอกระยะเวลาปลดล็อก 1–60 วินาที'};
  form.addEventListener('invalid', (event) => { if (setupMessages[event.target.id]) event.target.setCustomValidity(setupMessages[event.target.id]); }, true);
  fields.forEach((id) => document.getElementById(id).addEventListener('input', () => document.getElementById(id).setCustomValidity('')));

  // Keep setup state in one small adapter so pending recovery and activation
  // use the same storage boundary.
  const setupStorage = {
    key: 'smartlock.setup.v1',
    pendingKey: 'smartlock.setup.pending.v1',
    read() {
      try { return JSON.parse(localStorage.getItem(this.key) || 'null'); }
      catch (_) { return null; }
    },
    readPending() {
      try {
        let value = JSON.parse(localStorage.getItem(this.pendingKey) || 'null');
        if (!value) {
          const legacy = this.read();
          if (legacy && legacy.state === 'pending' && legacy.payload) {
            value = {state:'pending', payload:legacy.payload};
            localStorage.setItem(this.pendingKey, JSON.stringify(value));
          }
        }
        return value && value.state === 'pending' && value.payload ? value.payload : null;
      } catch (_) { return null; }
    },
    savePending(payload) {
      const {adminPin, pinConfirm, ...publicPending} = payload;
      localStorage.setItem(this.pendingKey, JSON.stringify({state: 'pending', payload: publicPending}));
    },
    markActive(payload) {
      localStorage.setItem(this.key, JSON.stringify({state: 'active', session: payload.session, credential: payload.credential, apSsid: payload.apSsid, apPassword: payload.apPassword}));
      localStorage.setItem('smartlock.devices.v1', JSON.stringify([{id:'D000001',credential:payload.credential,name:payload.ownerName}]));
      localStorage.removeItem(this.pendingKey);
      // Older versions stored pending state under the active key.
      const previous = this.read();
      if (previous && previous.state === 'pending') localStorage.removeItem(this.key);
    },
    clearPending() { localStorage.removeItem(this.pendingKey); }
  };

  function show(message, kind) {
    status.textContent = message;
    status.className = kind || 'note';
  }
  function showPendingNetwork(payload) {
    if (!payload || typeof payload.apPassword !== 'string' || !payload.apPassword) return;
    document.getElementById('pendingApPassword').textContent = payload.apPassword;
    document.getElementById('pendingNetworkDetails').hidden = false;
  }
  function lockFields(locked) {
    fields.forEach((id) => { document.getElementById(id).disabled = locked && id !== 'adminPin' && id !== 'pinConfirm'; });
    submitButton.textContent = locked ? 'ลองส่งคำขอตั้งค่าอีกครั้ง' : 'ตั้งค่าอุปกรณ์';
    restartButton.hidden = !locked;
  }
  function validPayload(payload) {
    const encoder = new TextEncoder();
    const byteLength = (value) => encoder.encode(value).length;
    const seconds = Number(payload.unlockSeconds);
    return /^[0-9]{4}$/.test(payload.adminPin) && payload.adminPin===payload.pinConfirm &&
      byteLength(payload.ownerName) >= 1 && byteLength(payload.ownerName) <= 40 &&
      Number.isInteger(seconds) && seconds >= 1 && seconds <= 60;
  }
  function randomHex(byteCount) {
    const bytes = new Uint8Array(byteCount);
    crypto.getRandomValues(bytes);
    return Array.from(bytes, (byte) => byte.toString(16).padStart(2, '0')).join('');
  }
  function formPayload() {
    const payload = {session: SESSION};
    fields.forEach((id) => { payload[id] = document.getElementById(id).value; });
    payload.unlockSeconds = String(Number(payload.unlockSeconds));
    return payload;
  }
  function restorePayload(payload) {
    fields.filter((id) => id !== 'adminPin' && id !== 'pinConfirm').forEach((id) => { document.getElementById(id).value = payload[id] || ''; });
    lockFields(true);
  }

  const saved = setupStorage.read();
  let pending = setupStorage.readPending();
  function activateSetup(payload, result) {
    if (!result || result.ok !== true || result.deviceId !== 'D000001' || result.role !== 'Owner') return false;
    payload.apSsid = typeof result.apSsid === 'string' ? result.apSsid : (payload.apSsid || '');
    setupStorage.markActive(payload);
    document.getElementById('pendingNetworkDetails').hidden = true;
    form.hidden = true;
    document.getElementById('networkDetails').hidden = false;
    document.getElementById('apSsid').textContent = payload.apSsid || '';
    document.getElementById('apPassword').textContent = payload.apPassword || '';
    pending = null;
    show('ตั้งค่าเสร็จแล้ว เบราว์เซอร์นี้ลงทะเบียนกับอุปกรณ์เรียบร้อย', 'ok');
    return true;
  }
  async function reconcileSetup(payload) {
    if (!payload || !/^[0-9a-f]{64}$/.test(payload.credential || '')) return false;
    try {
      const response = await fetch('/api/registration/reconcile', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},cache:'no-store',body:new URLSearchParams({kind:'setup',credential:payload.credential}).toString()});
      return response.ok && activateSetup(payload, await response.json());
    } catch (_) { return false; }
  }
  if (pending && await reconcileSetup(pending)) return;
  if (pending) showPendingNetwork(pending);
  if (!SESSION) {
    noSession.hidden = false;
    document.getElementById('intro').hidden = true;
    show('ต้องเปิดหน้านี้จากเซสชันตั้งค่า', 'note');
    return;
  }
  form.hidden = false;

  if (saved && saved.state === 'active' && saved.session === SESSION) {
    form.hidden = true;
    document.getElementById('networkDetails').hidden = false;
    document.getElementById('apSsid').textContent = saved.apSsid || 'ไม่พร้อมใช้งาน';
    document.getElementById('apPassword').textContent = saved.apPassword || 'ไม่พร้อมใช้งาน';
    show('เบราว์เซอร์นี้ลงทะเบียนกับอุปกรณ์แล้ว', 'ok');
    return;
  }
  if (pending) {
    if (pending.session === SESSION) {
      restorePayload(pending);
      show('มีคำขอตั้งค่าที่รอดำเนินการ การลองอีกครั้งจะส่งข้อมูลชุดเดิม หากต้องการเริ่มใหม่ ให้เลือกปุ่มเริ่มตั้งค่าใหม่', 'note');
    } else {
      // Keep the exact pending credential across QR refreshes. A current valid
      // Setup session will authorize the retry; never replace recoverable proof.
      pending.session = SESSION;
      try { setupStorage.savePending(pending); } catch (_) {}
      restorePayload(pending);
      show('ใช้ QR ตั้งค่าปัจจุบันส่งคำขอเดิมอีกครั้ง เพื่อเก็บข้อมูลยืนยันชุดเดิม', 'note');
    }
  }
  restartButton.addEventListener('click', () => {
    show('คำขอตั้งค่าที่ค้างอยู่ยังเก็บไว้เพื่อกู้คืนข้อมูลยืนยัน หากอุปกรณ์บันทึกสำเร็จแล้ว ให้ลองส่งคำขอเดิมอีกครั้ง', 'note');
  });

  form.addEventListener('submit', async (event) => {
    event.preventDefault();
    let payload;
    if (pending && pending.session === SESSION) {
      payload = {session:pending.session,ownerName:pending.ownerName,unlockSeconds:pending.unlockSeconds,credential:pending.credential,apPassword:pending.apPassword, adminPin:document.getElementById('adminPin').value,pinConfirm:document.getElementById('pinConfirm').value};
      if (!form.reportValidity() || !validPayload(payload)) {
        show('กรอกรหัสผ่านผู้ดูแลระบบอีกครั้งก่อนลองใหม่', 'error');
        return;
      }
    } else {
      payload = formPayload();
      if (!form.reportValidity() || !validPayload(payload)) {
        show('ตรวจสอบข้อจำกัดของช่องกรอกข้อมูลแล้วลองอีกครั้ง', 'error');
        return;
      }
      if (!window.crypto || typeof window.crypto.getRandomValues !== 'function') {
        show('เบราว์เซอร์นี้ไม่รองรับการสร้างข้อมูลสุ่มที่ปลอดภัย โปรดเปิดหน้านี้ด้วยเบราว์เซอร์ที่รองรับแล้วลองอีกครั้ง', 'error');
        return;
      }
      // Generate independent random values for browser authentication and the
      // protected setup access point. Save both before the first network call.
      payload.credential = randomHex(32);
      payload.apPassword = randomHex(16);
      try {
        setupStorage.savePending(payload);
      } catch (_) {
        show('เบราว์เซอร์นี้บันทึกคำขอตั้งค่าที่ค้างไว้ไม่ได้ จึงยังไม่ได้ส่งคำขอ โปรดเปิดใช้พื้นที่จัดเก็บในเบราว์เซอร์แล้วลองอีกครั้ง', 'error');
        return;
      }
      pending = payload;
      showPendingNetwork(pending);
      lockFields(true);
    }

    if (pending && await reconcileSetup(pending)) return;
    submitButton.disabled = true;
    show('กำลังส่งคำขอตั้งค่า...', 'note');
    try {
      const body = new URLSearchParams(payload).toString();
      const response = await fetch('/api/setup/complete', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body
      });
      if (response.status !== 200) {
        if (pending && await reconcileSetup(pending)) return;
        show('ยังไม่ได้รับการยืนยันการตั้งค่า (HTTP ' + response.status + ') บันทึกข้อมูลยืนยันที่รอดำเนินการไว้แล้ว ลองอีกครั้งเพื่อส่งคำขอเดิม', 'error');
        return;
      }
      let result;
      try { result = await response.json(); }
      catch (_) {
        if (pending && await reconcileSetup(pending)) return;
        show('อุปกรณ์ส่งข้อมูลตอบกลับที่อ่านไม่ได้ บันทึกข้อมูลยืนยันที่รอดำเนินการไว้แล้ว ลองอีกครั้งเพื่อส่งคำขอเดิม', 'error');
        return;
      }
      if (!result || typeof result !== 'object' || Array.isArray(result) || result.ok !== true) {
        if (pending && await reconcileSetup(pending)) return;
        show('อุปกรณ์ส่งข้อมูลตอบกลับที่ไม่คาดคิด บันทึกข้อมูลยืนยันที่รอดำเนินการไว้แล้ว ลองอีกครั้งเพื่อส่งคำขอเดิม', 'error');
        return;
      }
      // A successful completion is authoritative for the first Owner record.
      if (!activateSetup(payload, {ok:true, deviceId:'D000001', role:'Owner', apSsid:typeof result.apSsid === 'string' ? result.apSsid : ''})) {
        show('อุปกรณ์ยืนยันการตั้งค่าแล้ว แต่เบราว์เซอร์ยังบันทึกสถานะไม่ได้ คำขอที่ค้างไว้ยังกู้คืนได้', 'error');
        return;
      }
      pending = null;
      form.hidden = true;
      document.getElementById('networkDetails').hidden = false;
      document.getElementById('apSsid').textContent = payload.apSsid || 'ไม่พร้อมใช้งาน';
      document.getElementById('apPassword').textContent = payload.apPassword;
    } catch (_) {
      if (pending && await reconcileSetup(pending)) return;
      show('เชื่อมต่ออุปกรณ์ไม่ได้ บันทึกข้อมูลยืนยันที่รอดำเนินการไว้แล้ว ลองอีกครั้งเพื่อส่งคำขอเดิม', 'error');
    } finally {
      submitButton.disabled = false;
      payload.adminPin='';payload.pinConfirm='';if(pending){pending.adminPin='';pending.pinConfirm='';}document.getElementById('adminPin').value='';document.getElementById('pinConfirm').value='';
    }
  });
})();
</script>
</body></html>)HTML";

static const char kManage[] PROGMEM = R"HTML(<!doctype html>
<html lang="th"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="color-scheme" content="light"><title>จัดการ SmartLock</title>
<style>
:root{font:16px/1.45 system-ui,-apple-system,"Segoe UI",sans-serif;color:#17212b;background:#f3f6f8}*{box-sizing:border-box}body{margin:0;padding:1rem}main{width:min(100%,60rem);margin:2rem auto;background:#fff;border:1px solid #dbe3e8;border-radius:1rem;padding:clamp(1rem,4vw,2rem);box-shadow:0 8px 30px #102a3a12}h1{margin:0 0 .4rem}h2{font-size:1.15rem;margin:1.7rem 0 .6rem}p{color:#455564}label{display:block;font-weight:650;margin:.8rem 0 .3rem}input,select,button{font:inherit;min-height:2.7rem;padding:.55rem .7rem;border:1px solid #9aaab5;border-radius:.5rem}input,select{width:100%;background:#fff;color:#17212b}button{border:0;background:#075985;color:#fff;font-weight:650;cursor:pointer}button:disabled{opacity:.6;cursor:wait}.secondary{background:#e8eef2;color:#17212b}.formrow{display:grid;grid-template-columns:2fr 1fr auto;gap:.55rem;align-items:end}.formrow button{min-width:8rem}.card{border:1px solid #dbe3e8;border-radius:.65rem;padding:.85rem;margin:.55rem 0}.card p{margin:.2rem 0}.card button{min-height:2.3rem;margin-top:.5rem}.deviceCard{margin:.5rem 0 0 1rem;padding:.65rem;border-left:3px solid #cbd5dc;background:#f8fafb;border-radius:.4rem}.enrollQr{display:block;width:min(100%,20rem);height:auto;margin:.7rem auto;border:1px solid #dbe3e8;border-radius:.5rem}.roleActions{display:flex;gap:.45rem;flex-wrap:wrap}.roleActions button{width:auto}.lineButton{display:inline-block;margin:.2rem 0 .8rem;padding:.65rem .9rem;border-radius:.5rem;background:#075985;color:#fff;font-weight:650;text-decoration:none}.friendButton{display:block;text-align:center;background:#06c755;min-height:3rem;font-size:1.05rem}.unitRow{display:flex;align-items:center;gap:.55rem}.unitRow input{width:7rem}.settingsForm button{width:100%;margin-top:1rem}#unlockResult,#adminPinResult{padding:.65rem;border-radius:.5rem}#unlockResult:empty,#adminPinResult:empty{display:none}#actionDialog{position:fixed;inset:0;z-index:1000;margin:0;padding:1rem;max-width:none;border:0;border-radius:0;background:#17212b99;display:grid;place-items:center}#actionDialog .dialogPanel{width:min(100%,32rem);background:#fff;border:1px solid #dbe3e8;border-radius:.8rem;padding:1rem;box-shadow:0 16px 48px #0004}.muted{color:#61717d;font-size:1rem}.manageNav{display:flex;gap:.45rem;flex-wrap:wrap;margin:1rem 0;padding:.35rem;background:#f3f6f8;border-radius:.75rem}.manageNav a{display:inline-block;padding:.55rem .7rem;border-radius:.5rem;background:#e8eef2;color:#075985;font-weight:650;text-decoration:none}.manageNav a[aria-current="page"]{background:#075985;color:#fff}.dashboardGrid{display:grid;grid-template-columns:repeat(auto-fit,minmax(10rem,1fr));gap:.6rem;margin:.8rem 0}.dashboardGrid>div{display:flex;flex-direction:column;gap:.15rem;border:1px solid #dbe3e8;border-radius:.55rem;padding:.75rem;min-height:5rem}.dashboardGrid strong{overflow-wrap:anywhere}.networkGrid{display:grid;grid-template-columns:repeat(auto-fit,minmax(10rem,1fr));gap:.6rem;margin:.8rem 0}.networkGrid>div{display:flex;flex-direction:column;gap:.15rem;border:1px solid #dbe3e8;border-radius:.55rem;padding:.65rem}.networkList{margin:.6rem 0}.networkChoice{display:flex;align-items:center;gap:.65rem;border:1px solid #dbe3e8;border-radius:.55rem;padding:.65rem;margin:.4rem 0}.networkChoice input{width:auto;min-height:auto}.networkChoice span{flex:1}#networkResult{padding:.75rem;border-radius:.5rem;white-space:pre-wrap}#networkResult:empty{display:none}#status{padding:.75rem;border-radius:.5rem;white-space:pre-wrap}#status:empty{display:none}.error{background:#fff0ed;color:#842b1c}.ok{background:#eaf7ee;color:#17552d}.note{background:#eef6fc;color:#174a70}[hidden]{display:none!important}@media(max-width:38rem){.formrow{grid-template-columns:1fr}.formrow button{width:100%}.manageNav a{flex:1 1 42%;text-align:center}}
</style></head><body><main>
<h1>จัดการ SmartLock</h1><p>จัดการอุปกรณ์ที่ได้รับสิทธิ์เข้าใช้ SmartLock นี้</p>
<section id="unavailable" hidden><p id="unavailableText"></p><p><a href="/">กลับไปหน้าหลักของอุปกรณ์</a></p></section>
<section id="app" hidden>
  <nav class="manageNav" aria-label="หน้าจัดการ"><a href="#dashboard" data-page-link="dashboard">ภาพรวม</a><a href="#management" data-page-link="management">อุปกรณ์</a><a href="#network" data-page-link="network">เครือข่าย</a><a id="lineNav" href="#line" data-page-link="line" hidden>LINE</a><a id="unlockNav" href="#unlock" data-page-link="unlock" hidden>ระยะเวลาปลดล็อก</a><a id="adminPinNav" href="#admin-pin" data-page-link="admin-pin" hidden>เปลี่ยนรหัส Admin PIN</a><a href="#system" data-page-link="system">ระบบ</a></nav>
  <section id="dashboardPanel" class="managementPage">
    <h2>ภาพรวม SmartLock</h2><p class="muted">ข้อมูลสถานะจากอุปกรณ์จริง อัปเดตเมื่อเปิดหน้านี้หรือกดรีเฟรช</p><button id="refreshDashboard" type="button" class="secondary">รีเฟรชข้อมูล</button>
    <div class="dashboardGrid" aria-live="polite"><div><span class="muted">สถานะกลอน</span><strong data-summary="locked">—</strong></div><div><span class="muted">เครือข่าย</span><strong id="dashboardNetworkMode">—</strong></div><div><span class="muted">IP อุปกรณ์</span><strong id="dashboardIp">—</strong></div><div><span class="muted">ตัวตนที่ลงทะเบียน</span><strong data-summary="identities">—</strong></div><div><span class="muted">ตัวตนที่ใช้งาน</span><strong data-summary="activeIdentities">—</strong></div><div><span class="muted">ตัวตนที่เพิกถอน</span><strong data-summary="revokedIdentities">—</strong></div><div><span class="muted">เวลาเปิดทำงาน</span><strong data-summary="uptimeMs">—</strong></div><div><span class="muted">หน่วยความจำว่าง</span><strong data-summary="freeHeap">—</strong></div><div><span class="muted">หน่วยความจำว่างต่ำสุด</span><strong data-summary="minFreeHeap">—</strong></div><div><span class="muted">ฐานข้อมูล</span><strong data-summary="databaseHealthy">—</strong></div><div><span class="muted">เฟิร์มแวร์</span><strong data-summary="firmware">—</strong></div></div>
    <p id="dashboardResult" role="status" aria-live="polite"></p>
  </section>
  <section id="managementPanel" class="managementPage" hidden>
  <h2>อุปกรณ์ที่ได้รับสิทธิ์</h2><button id="toggleIdentityForm" type="button" aria-expanded="false">+ เพิ่มอุปกรณ์</button><form id="identityForm" hidden><div class="formrow"><label>ชื่อ<input id="identityName" required maxlength="40" autocomplete="name"><span class="muted">ไม่เกิน 40 ไบต์ UTF-8</span></label><label>สิทธิ์<select id="identityRole"></select></label><button type="submit">สร้าง QR ลงทะเบียน</button></div></form><p id="enrollInstruction" class="muted" hidden></p><div id="identities" aria-live="polite"></div>
  </section>
  <section id="networkPanel" class="managementPage" hidden>
    <h2>เครือข่าย</h2><p class="muted">ใช้งานปกติผ่าน Wi-Fi บ้าน จุดกระจายสัญญาณจะปิดหลังยืนยันการเข้าถึงผ่าน LAN แล้ว 3 วินาที หาก Wi-Fi บ้านขาดการเชื่อมต่อ 60 วินาที ระบบจะเปิดจุดกระจายสัญญาณกู้คืนที่มีรหัสผ่าน</p>
    <div class="networkGrid" aria-live="polite"><div><span class="muted">โหมดปัจจุบัน</span><strong id="networkMode">—</strong></div><div><span class="muted">นโยบายเครือข่าย</span><strong id="networkPolicy">—</strong></div><div><span class="muted">จุดกระจายสัญญาณ</span><strong id="networkApEnabled">—</strong></div><div><span class="muted">SSID ของจุดกระจายสัญญาณ</span><strong id="networkApSsid">—</strong></div><div><span class="muted">IP ของจุดกระจายสัญญาณ</span><strong id="networkApIp">—</strong></div><div><span class="muted">สถานะการเชื่อมต่อ</span><strong id="networkStaState">—</strong></div><div><span class="muted">SSID ที่เชื่อมต่อ</span><strong id="networkStaSsid">—</strong></div><div><span class="muted">IP ของอุปกรณ์ / IP ใน LAN</span><strong id="networkStaIp">—</strong></div><div><span class="muted">ความแรงสัญญาณ</span><strong id="networkRssi">—</strong></div></div>
    <h2>เครือข่าย Wi-Fi ที่พบ</h2><button id="scanNetworks" type="button">ค้นหา Wi-Fi</button><div id="networkList" class="networkList" aria-live="polite"></div>
    <form id="networkConnectForm"><label for="networkPassword">รหัสผ่าน Wi-Fi</label><input id="networkPassword" type="password" autocomplete="new-password" autocapitalize="off"><button id="networkConnect" type="submit">ทดสอบและเชื่อมต่อ</button></form><p id="canonicalLinkRow" hidden><a id="canonicalAnchor">เปิด SmartLock ผ่านชื่อเดิม</a></p><p class="muted">เมื่อเปลี่ยนไปใช้ Wi-Fi บ้าน ให้เปิดชื่อ SmartLock เดิมในเบราว์เซอร์เครื่องเดิม ระบบจะใช้ข้อมูลยืนยันเดิมโดยไม่ต้องส่งต่อสิทธิ์</p><p id="lanEnrollmentHelp" class="muted" hidden></p><p id="networkResult" role="status" aria-live="polite"></p>
  </section>
          <section id="linePanel" class="managementPage" hidden><h2>LINE</h2><div class="dashboardGrid" aria-live="polite"><div><span class="muted">LINE</span><strong id="lineReady">—</strong></div><div><span class="muted">การแจ้งเตือน</span><strong id="lineEnabled">—</strong></div></div><a id="lineAddFriend" class="lineButton friendButton" hidden target="_blank" rel="noopener noreferrer">เพิ่มเพื่อน LINE</a><p id="lineFriendHelp" class="muted">กำลังโหลดข้อมูล…</p><p class="muted">การแจ้งเตือนจะถูกส่งไปยังเพื่อนทุกคนของบัญชี LINE Official Account นี้ กดเพิ่มเพื่อน LINE เพื่อรับการแจ้งเตือนจาก SmartLock</p><p id="lineSetupState" class="muted">กำลังโหลดสถานะ…</p><p id="lineSetupHelp" class="muted" hidden>ยังไม่ได้ตั้งค่า LINE ให้ผู้ดูแลระบบช่วยตั้งค่าหลังยืนยันรหัสผู้ดูแลระบบ</p><form id="lineLabelForm" hidden><label for="lineLabel">ชื่ออุปกรณ์ (ไม่เกิน 64 ไบต์)</label><input id="lineLabel" maxlength="64" autocomplete="off" required><button id="lineRename" type="submit">เปลี่ยนชื่ออุปกรณ์</button></form><div id="lineStatusGrid" class="dashboardGrid" aria-live="polite"><div><span class="muted">สถานะการส่ง</span><strong id="lineState">—</strong></div><div><span class="muted">โควตาข้อความ LINE เดือนนี้</span><strong id="lineQuota">—</strong></div><div><span class="muted">คิวรอส่ง</span><strong id="lineQueued">—</strong></div><div><span class="muted">ส่งสำเร็จในการทำงานนี้</span><strong id="lineSent">—</strong></div><div><span class="muted">ส่งไม่สำเร็จในการทำงานนี้</span><strong id="lineFailed">—</strong></div><div><span class="muted">ชื่ออุปกรณ์</span><strong id="lineSavedLabel">—</strong></div></div><div class="roleActions"><button id="lineTest" type="button" class="secondary" hidden>ส่งข้อความทดสอบถึงเพื่อนทุกคน</button><button id="lineDisconnect" type="button" class="secondary" hidden>ยกเลิกการเชื่อมต่อ</button><button id="lineArm" type="button" class="secondary">ตั้งค่า LINE ใหม่</button><button id="lineRefresh" type="button" class="secondary">รีเฟรชสถานะ</button></div><form id="lineMaintenanceForm" hidden><label for="lineMaintenancePin">ยืนยันรหัสผู้ดูแลระบบ</label><input id="lineMaintenancePin" name="pin" type="password" inputmode="numeric" pattern="[0-9]{4}" maxlength="4" required autocomplete="off"><button type="submit">ยืนยัน</button><button id="lineCancelMaintenance" type="button" class="secondary">ยกเลิก</button></form><p id="lineMaintenanceHelp" class="muted" hidden></p><p id="lineResult" role="status" aria-live="polite"></p></section><section id="unlockPanel" class="managementPage" hidden><h2>ระยะเวลาปลดล็อก</h2><p class="muted">เวลาที่กลอนปลดล็อกหลังยืนยันสิทธิ์สำเร็จ ก่อนล็อกกลับอัตโนมัติ การบันทึกไม่ปลดล็อกประตู</p><p>ค่าปัจจุบัน: <strong id="unlockCurrent">—</strong> วินาที</p><form id="unlockForm" class="settingsForm"><label for="unlockSeconds">ระยะเวลาใหม่ (1–60 วินาที)</label><div class="unitRow"><input id="unlockSeconds" name="seconds" type="number" inputmode="numeric" min="1" max="60" step="1" required><span>วินาที</span></div><button type="submit">บันทึก</button></form><p id="unlockResult" role="status" aria-live="polite"></p></section><section id="adminPinPanel" class="managementPage" hidden><h2>เปลี่ยนรหัส Admin PIN</h2><p class="muted">รหัส Admin PIN ใช้กับเมนูผู้ดูแลบนหน้าจอล็อก กรอกผิดครบ 3 ครั้งต้องรอ 60 วินาที</p><form id="adminPinForm" class="settingsForm"><label>รหัส Admin PIN ปัจจุบัน<input name="current" type="password" inputmode="numeric" pattern="[0-9]{4}" minlength="4" maxlength="4" required autocomplete="off"></label><label>รหัสใหม่ (ตัวเลข 4 หลัก)<input name="next" type="password" inputmode="numeric" pattern="[0-9]{4}" minlength="4" maxlength="4" required autocomplete="new-password"></label><label>ยืนยันรหัสใหม่<input name="confirm" type="password" inputmode="numeric" pattern="[0-9]{4}" minlength="4" maxlength="4" required autocomplete="new-password"></label><button type="submit">เปลี่ยนรหัส</button></form><p id="adminPinResult" role="status" aria-live="polite"></p></section><section id="systemPanel" class="managementPage" hidden><h2>ระบบ</h2><p class="muted">ข้อมูลระบบจริงชุดเดียวกับภาพรวม</p><button id="refreshSystem" type="button" class="secondary">รีเฟรชข้อมูลระบบ</button><div class="dashboardGrid" aria-live="polite"><div><span class="muted">เวลาเปิดทำงาน</span><strong data-summary="uptimeMs">—</strong></div><div><span class="muted">หน่วยความจำว่าง</span><strong data-summary="freeHeap">—</strong></div><div><span class="muted">หน่วยความจำว่างต่ำสุด</span><strong data-summary="minFreeHeap">—</strong></div><div><span class="muted">ฐานข้อมูล</span><strong data-summary="databaseHealthy">—</strong></div><div><span class="muted">เฟิร์มแวร์</span><strong data-summary="firmware">—</strong></div><div><span class="muted">ชื่ออุปกรณ์หลัก</span><strong data-summary="canonicalHost">—</strong></div></div>
<p id="systemResult" role="status" aria-live="polite"></p></section>
</section><section id="actionDialog" role="dialog" aria-modal="true" aria-labelledby="actionDialogTitle" hidden><div class="dialogPanel"><h2 id="actionDialogTitle">ยืนยันการดำเนินการ</h2><p id="actionDialogText"></p><div class="roleActions"><button id="actionDialogConfirm" type="button">ยืนยัน</button><button id="actionDialogCancel" type="button" class="secondary">ยกเลิก</button></div></div></section><section id="enrollResult" class="card" hidden aria-live="polite"><h2>ลงทะเบียนอุปกรณ์</h2><p id="enrollSummary"></p><img id="enrollQr" class="enrollQr" alt="QR สำหรับลงทะเบียน"><p id="enrollExpiry" class="muted"></p><p><a id="enrollUrl" rel="noopener">เปิดหน้าลงทะเบียน</a></p></section><p id="status" role="status" aria-live="polite"></p>
</main><script>
(() => {'use strict';
  const status=document.getElementById('status'), app=document.getElementById('app'), unavailable=document.getElementById('unavailable');
  const identityNameInput=document.getElementById('identityName');identityNameInput.addEventListener('invalid',()=>identityNameInput.setCustomValidity('กรอกชื่ออุปกรณ์ที่ได้รับสิทธิ์'));identityNameInput.addEventListener('input',()=>identityNameInput.setCustomValidity(''));
  let token='',authenticatedDevice=null;
  function show(message,kind){status.textContent=message;status.className=kind||'note';}
  function confirmInline(title,message){return new Promise(resolve=>{const box=document.getElementById('actionDialog'),confirm=document.getElementById('actionDialogConfirm'),cancel=document.getElementById('actionDialogCancel');document.getElementById('actionDialogTitle').textContent=title;document.getElementById('actionDialogText').textContent=message;box.hidden=false;const finish=value=>{box.hidden=true;confirm.removeEventListener('click',yes);cancel.removeEventListener('click',no);resolve(value);};const yes=()=>finish(true),no=()=>finish(false);confirm.addEventListener('click',yes);cancel.addEventListener('click',no);cancel.focus();});}
  function formBody(fields){const body=new URLSearchParams();Object.keys(fields).forEach((key)=>body.set(key,String(fields[key])));return body.toString();}
  async function request(path,fields,method){const requestedAt=Date.now();const verb=method||'POST',options={method:verb};if(verb!=='GET'){options.headers={'Content-Type':'application/x-www-form-urlencoded'};options.body=formBody(fields);}const response=await fetch(path,options);let data=null;try{data=await response.json();}catch(_){}if(response.status===403&&token&&data&&data.error==='forbidden'){token='';stopNetworkPoll();unavailableMessage('เซสชันผู้ดูแลหมดอายุหรือไม่มีสิทธิ์เข้าถึงแล้ว กลับไปที่หน้าจอล็อกและสแกน QR สำหรับจัดการใหม่');throw new Error('unauthorized');}if(!response.ok||!data||data.ok===false){const error=new Error('request');error.code=data&&data.error;error.status=response.status;throw error;}data.requestedAt=requestedAt;return data;}
  function unavailableMessage(message){app.hidden=true;unavailable.hidden=false;document.getElementById('unavailableText').textContent=message;}
  function card(text,detail,buttonText,onClick){const el=document.createElement('article');el.className='card';const title=document.createElement('strong');title.textContent=text;el.appendChild(title);if(detail){const line=document.createElement('p');line.className='muted';line.textContent=detail;el.appendChild(line);}if(buttonText){const button=document.createElement('button');button.type='button';button.className='secondary';button.textContent=buttonText;button.addEventListener('click',onClick);el.appendChild(button);}return el;}
  function validateState(data){return data&&Array.isArray(data.identities)&&(data.actorRole==='Owner'||data.actorRole==='Admin')&&typeof data.lanEnrollment==='boolean';}
  let actorRole='',lanEnrollment=false,enrollExpiryTimer=0,enrollExpiryTick=()=>{};
  window.addEventListener('focus',()=>enrollExpiryTick());
  document.addEventListener('visibilitychange',()=>enrollExpiryTick());
  const roleLabels={Owner:'เจ้าของ',Admin:'ผู้ดูแลระบบ',User:'ผู้ใช้',Guest:'ผู้เยี่ยมชม'};
  function allowedRoles(){return actorRole==='Owner'?['User','Guest','Admin']:['User','Guest'];}
  function configureRoleOptions(){const select=document.getElementById('identityRole');select.replaceChildren();allowedRoles().forEach((role)=>{const option=document.createElement('option');option.value=role;option.textContent=roleLabels[role];select.appendChild(option);});}
  function bytesUtf8(value){return new TextEncoder().encode(value).length;}
  function showEnrollment(result,identity){if(enrollExpiryTimer)clearInterval(enrollExpiryTimer);if(!result||typeof result.url!=='string'||typeof result.qrSvg!=='string'||result.qrSvg.length>24000||result.expiresIn!==120)throw new Error('enrollment');const link=new URL(result.url,location.href),canonical=/^smartlock-[0-9a-f]{12}\.local$/i.test(link.hostname);if((link.protocol!=='http:'&&link.protocol!=='https:')||link.username||link.password||link.hash||(link.origin!==location.origin&&!canonical)||link.pathname!=='/enroll'||!link.searchParams.get('session'))throw new Error('enrollment_url');const image='data:image/svg+xml;charset=utf-8,'+encodeURIComponent(result.qrSvg),box=document.getElementById('enrollResult');document.getElementById('enrollQr').src=image;document.getElementById('enrollUrl').href=link.href;document.getElementById('enrollSummary').textContent=identity.name+' · สิทธิ์ '+(roleLabels[identity.role]||identity.role||'ผู้ใช้');box.hidden=false;const deadline=result.requestedAt+result.expiresIn*1000;const expiry=document.getElementById('enrollExpiry');const tick=()=>{const remaining=Math.max(0,Math.ceil((deadline-Date.now())/1000));expiry.textContent=remaining>0?'QR หมดอายุใน '+remaining+' วินาที':'QR หมดอายุแล้ว กรุณาสร้าง QR ใหม่';if(remaining<=0){clearInterval(enrollExpiryTimer);enrollExpiryTimer=0;document.getElementById('enrollQr').hidden=true;document.getElementById('enrollUrl').hidden=true;}};document.getElementById('enrollQr').hidden=false;document.getElementById('enrollUrl').hidden=false;enrollExpiryTick=tick;enrollExpiryTimer=setInterval(tick,1000);tick();box.scrollIntoView({behavior:'smooth',block:'nearest'});}
  const networkPanel=document.getElementById('networkPanel'),networkResult=document.getElementById('networkResult'),dashboardResult=document.getElementById('dashboardResult'),systemResult=document.getElementById('systemResult');
  const ownerPages=['line','unlock','admin-pin'];const managementPages={dashboard:document.getElementById('dashboardPanel'),management:document.getElementById('managementPanel'),network:networkPanel,line:document.getElementById('linePanel'),unlock:document.getElementById('unlockPanel'),'admin-pin':document.getElementById('adminPinPanel'),system:document.getElementById('systemPanel')};
  const lineResult=document.getElementById('lineResult');
  function lineMessage(message,kind){lineResult.textContent=message;lineResult.className=kind||'note';}
  function lineCount(value){return Number.isSafeInteger(value)&&value>=0?String(value):'—';}
  function renderLineStatus(data){
    if(!data||typeof data.configured!=='boolean'||typeof data.enabled!=='boolean'||typeof data.staConnected!=='boolean'||typeof data.quotaKnown!=='boolean'||typeof data.deviceLabel!=='string'||typeof data.publicBasicId!=='string'||!Number.isSafeInteger(data.configVersion)||!Number.isSafeInteger(data.testState))throw new Error('line_status');
    document.getElementById('lineSetupState').textContent=data.configured?'ตั้งค่าแล้ว':'ยังไม่ได้ตั้งค่า';
    document.getElementById('lineReady').textContent=data.state==='ready'?'พร้อมใช้งาน':'ยังไม่พร้อม';document.getElementById('lineEnabled').textContent=data.enabled?'เปิด':'ปิด';
    document.getElementById('lineSetupHelp').hidden=data.configured;
    document.getElementById('lineLabelForm').hidden=!data.configured;
    document.getElementById('lineStatusGrid').hidden=!data.configured;
    document.getElementById('lineTest').hidden=!data.configured;
    document.getElementById('lineDisconnect').hidden=!data.configured;
    const addFriend=document.getElementById('lineAddFriend'),validBasicId=/^@[A-Za-z0-9._-]{1,63}$/.test(data.publicBasicId);
    document.getElementById('lineFriendHelp').textContent=validBasicId?'กดเพิ่มเพื่อน LINE เพื่อรับการแจ้งเตือนจาก SmartLock':'ยังไม่มีการตั้งค่า LINE ในอุปกรณ์นี้ จึงยังเพิ่มเพื่อนไม่ได้';
    addFriend.hidden=!validBasicId;if(validBasicId)addFriend.href='https://line.me/R/ti/p/'+encodeURIComponent(data.publicBasicId);else addFriend.removeAttribute('href');
    document.getElementById('lineState').textContent=({disabled:'ปิดอยู่',ready:'พร้อมใช้งาน',offline:'ไม่มี Wi-Fi',quota_full:'โควตาข้อความ LINE เดือนนี้เต็มแล้ว',auth_required:'ต้องตั้งค่า LINE ใหม่',service_error:'บริการขัดข้อง',time_unavailable:'เวลายังไม่พร้อม'})[data.state]||'ไม่ทราบสถานะ';
    document.getElementById('lineQuota').textContent=data.quotaKnown?lineCount(data.quotaUsed)+' / '+lineCount(data.quotaLimit):'ไม่ทราบ';
    document.getElementById('lineQueued').textContent=lineCount(data.queued);document.getElementById('lineSent').textContent=lineCount(data.sentThisRun);document.getElementById('lineFailed').textContent=lineCount(data.failedThisRun);document.getElementById('lineSavedLabel').textContent=data.deviceLabel||'—';document.getElementById('lineLabel').value=data.deviceLabel;
    document.getElementById('lineTest').disabled=!data.configured||!data.enabled||!data.staConnected;document.getElementById('lineDisconnect').disabled=!data.configured;
  }
  async function fetchLineStatus(){const data=await request('/api/line/status',{token});renderLineStatus(data);return data;}
  async function refreshLineStatus(){try{await fetchLineStatus();lineMessage('อัปเดตสถานะแล้ว','ok');}catch(_){if(token)lineMessage('โหลดสถานะ LINE ไม่ได้ ตรวจสอบการเชื่อมต่อแล้วลองอีกครั้ง','error');}}
  document.getElementById('lineLabelForm').addEventListener('submit',async(event)=>{event.preventDefault();const form=event.currentTarget,label=document.getElementById('lineLabel'),button=document.getElementById('lineRename');if(!form.reportValidity())return;button.disabled=true;try{await request('/api/line/label',{token,label:label.value.trim()});lineMessage('เปลี่ยนชื่ออุปกรณ์แล้ว','ok');await fetchLineStatus();}catch(_){lineMessage('เปลี่ยนชื่ออุปกรณ์ไม่ได้ ตรวจสอบชื่อแล้วลองอีกครั้ง','error');}finally{button.disabled=false;}});
  document.getElementById('lineTest').addEventListener('click',async(event)=>{const button=event.currentTarget;button.disabled=true;lineMessage('กำลังจัดคิวข้อความทดสอบ…','note');try{await fetchLineStatus();await request('/api/line/test',{token});let after=await fetchLineStatus();for(let i=0;i<12&&after.testState<2;i++){await new Promise(resolve=>setTimeout(resolve,250));after=await fetchLineStatus();}if(after.testState===2)lineMessage('LINE รับคำขอแล้ว เพื่อนทุกคนของบัญชี LINE นี้จะได้รับข้อความทดสอบ','ok');else if(after.testState===3)lineMessage('LINE ไม่รับคำขอทดสอบ ตรวจสอบสถานะและโควตา','error');else lineMessage('เพิ่มข้อความทดสอบในคิวแล้ว กำลังรอส่ง','note');}catch(_){lineMessage('ส่งข้อความทดสอบไม่ได้ ตรวจสอบสถานะ LINE แล้วลองอีกครั้ง','error');}finally{button.disabled=false;}});
  document.getElementById('lineDisconnect').addEventListener('click',async(event)=>{const button=event.currentTarget;if(!await confirmInline('ยกเลิกการเชื่อมต่อ LINE','ต้องการลบการตั้งค่า LINE ในอุปกรณ์หรือไม่ การดำเนินการนี้ลบเฉพาะการเชื่อมต่อในอุปกรณ์ ไม่เปลี่ยนแปลงบัญชี LINE'))return;button.disabled=true;try{await request('/api/line/disconnect',{token});lineMessage('ลบการตั้งค่า LINE ในอุปกรณ์แล้ว บัญชี LINE ยังไม่เปลี่ยนแปลง','ok');await fetchLineStatus();}catch(_){lineMessage('ยกเลิกการเชื่อมต่อ LINE ไม่ได้ ลองอีกครั้ง','error');}finally{button.disabled=false;}});
  let lineMaintenanceNonce='',lineMaintenanceTimer=0;
  function clearLineMaintenanceUi(){lineMaintenanceNonce='';clearTimeout(lineMaintenanceTimer);lineMaintenanceTimer=0;document.getElementById('lineMaintenanceForm').reset();}
  document.getElementById('lineArm').addEventListener('click',()=>{clearLineMaintenanceUi();document.getElementById('lineMaintenanceHelp').hidden=true;document.getElementById('lineMaintenanceForm').hidden=false;document.getElementById('lineMaintenancePin').focus();});
  document.getElementById('lineMaintenanceForm').addEventListener('submit',async(event)=>{
    event.preventDefault();const form=event.currentTarget;if(!form.reportValidity())return;
    let pin=form.elements.pin.value;form.reset();const button=form.querySelector('button[type="submit"]');button.disabled=true;
    try{const armed=await request('/api/line/maintenance/arm',{token,pin});
      if(armed.ok!==true||!/^([0-9a-f]{32})$/.test(armed.nonce)||armed.expiresIn!==120)throw new Error('response');
      lineMaintenanceNonce=armed.nonce;form.hidden=true;const help=document.getElementById('lineMaintenanceHelp');help.hidden=false;help.textContent='อนุญาตตั้งค่า LINE แล้ว รอผู้ดูแลระบบดำเนินการภายใน 120 วินาที';
      clearTimeout(lineMaintenanceTimer);lineMaintenanceTimer=setTimeout(()=>{clearLineMaintenanceUi();help.textContent='หมดเวลาตั้งค่า LINE กรุณายืนยันรหัสอีกครั้ง';},120000);
      lineMessage('ยืนยันรหัสผู้ดูแลระบบแล้ว','ok');
    }catch(_){clearLineMaintenanceUi();lineMessage('ยืนยันรหัสไม่ได้ รหัสอาจไม่ถูกต้องหรืออยู่ระหว่างพักการใช้งาน','error');}
    finally{pin='';button.disabled=false;}
  });
  document.getElementById('lineCancelMaintenance').addEventListener('click',async()=>{clearLineMaintenanceUi();document.getElementById('lineMaintenanceForm').hidden=true;document.getElementById('lineMaintenanceHelp').hidden=true;try{await request('/api/line/maintenance/cancel',{token});}catch(_){} });
  window.addEventListener('pagehide',clearLineMaintenanceUi);
  let networkPollTimer=0,selectedNetwork='';
  function showNetworkResult(message,kind){networkResult.textContent=message;networkResult.className=kind||'note';}
  function networkValue(value){return typeof value==='string'&&value.length?value:'—';}
  function renderNetworkStatus(data){
    const modeLabels={STA:'เชื่อมต่อ Wi-Fi บ้าน',AP_STA:'จุดกระจายสัญญาณและ Wi-Fi บ้าน',AP:'จุดกระจายสัญญาณ'};document.getElementById('networkMode').textContent=modeLabels[data.mode]||networkValue(data.mode);const policyLabels={home_lan:'เครือข่ายภายในบ้าน',setup_or_recovery:'ตั้งค่าหรือกู้คืน'};document.getElementById('networkPolicy').textContent=policyLabels[data.policy]||networkValue(data.policy);document.getElementById('networkApEnabled').textContent=typeof data.apEnabled==='boolean'?(data.apEnabled?'เปิด':'ปิด'):'—';const help=document.getElementById('lanEnrollmentHelp');help.hidden=lanEnrollment;help.textContent='การลงทะเบียนอุปกรณ์ที่ได้รับสิทธิ์ใช้ได้เมื่อโทรศัพท์กับ SmartLock อยู่บน Wi-Fi บ้านเครือข่ายเดียวกัน โหมดจุดกระจายสัญญาณใช้ตั้งค่าหรือกู้คืนเท่านั้น';
    document.getElementById('networkApSsid').textContent=networkValue(data.apSsid);
    document.getElementById('networkApIp').textContent=networkValue(data.apIp);
    const stateLabels={connected:'เชื่อมต่อแล้ว',disconnected:'ไม่ได้เชื่อมต่อ',connecting:'กำลังเชื่อมต่อ...',failed:'เชื่อมต่อไม่สำเร็จ'};const state=String(data.staState||'').toLowerCase();document.getElementById('networkStaState').textContent=stateLabels[state]||networkValue(data.staState);
    document.getElementById('networkStaSsid').textContent=networkValue(data.staSsid);
    document.getElementById('networkStaIp').textContent=networkValue(data.staIp);
    document.getElementById('networkRssi').textContent=data.rssi!==null&&data.rssi!==undefined&&Number.isFinite(Number(data.rssi))?String(data.rssi)+' dBm':'—';
    const connected=String(data.staState||'').toLowerCase()==='connected'&&typeof data.staIp==='string'&&data.staIp.length>0;
    const canonicalHost=typeof data.canonicalHost==='string'?data.canonicalHost:'';
    const canonicalOrigin=typeof data.canonicalOrigin==='string'?data.canonicalOrigin:'';
    const canonicalRow=document.getElementById('canonicalLinkRow'),canonicalAnchor=document.getElementById('canonicalAnchor');
    try{const origin=new URL(canonicalOrigin);if(origin.protocol==='http:'&&origin.hostname.toLowerCase()===canonicalHost.toLowerCase()&&/^smartlock-[0-9a-f]{12}\.local$/i.test(canonicalHost)){canonicalAnchor.href=origin.origin+'/';canonicalRow.hidden=false;}else canonicalRow.hidden=true;}catch(_){canonicalRow.hidden=true;}
  }
  async function networkRequest(path,fields){return request(path,Object.assign({token},fields||{}));}
  async function refreshNetworkStatus(){const data=await networkRequest('/api/network/status');renderNetworkStatus(data);return data;}
  function formatUptime(value){if(typeof value!=='number'||!Number.isFinite(value)||value<0)return '—';let seconds=Math.floor(value/1000);const days=Math.floor(seconds/86400);seconds%=86400;const hours=Math.floor(seconds/3600);seconds%=3600;const minutes=Math.floor(seconds/60);seconds%=60;return (days?days+' วัน ':'')+(hours?hours+' ชม. ':'')+(minutes?minutes+' นาที ':'')+seconds+' วินาที';}
  function formatBytes(value){return typeof value==='number'&&Number.isFinite(value)&&value>=0?value.toLocaleString('th-TH')+' ไบต์':'—';}
  function renderSummary(data){const labels={locked:typeof data.locked==='boolean'?(data.locked?'ล็อกอยู่':'ปลดล็อกอยู่'):'—',uptimeMs:formatUptime(data.uptimeMs),freeHeap:formatBytes(data.freeHeap),minFreeHeap:formatBytes(data.minFreeHeap),databaseHealthy:typeof data.databaseHealthy==='boolean'?(data.databaseHealthy?'ปกติ':'ผิดปกติ'):'—',identities:Number.isFinite(data.identities)?data.identities.toLocaleString('th-TH'):'—',activeIdentities:Number.isFinite(data.activeIdentities)?data.activeIdentities.toLocaleString('th-TH'):'—',revokedIdentities:Number.isFinite(data.revokedIdentities)?data.revokedIdentities.toLocaleString('th-TH'):'—',firmware:data.firmware==='Single identity checkpoint'?'ระบบตัวตนเดียว':data.firmware==='Phase 11 local audit'?'เฟส 11 บันทึกเหตุการณ์':data.firmware==='Phase 8 dashboard'?'\u0e40\u0e1f\u0e2a 8 \u0e41\u0e14\u0e0a\u0e1a\u0e2d\u0e23\u0e4c\u0e14':typeof data.firmware==='string'&&data.firmware?data.firmware:'—',canonicalHost:typeof data.canonicalHost==='string'&&data.canonicalHost?data.canonicalHost:'—'};document.querySelectorAll('[data-summary]').forEach((element)=>{element.textContent=labels[element.dataset.summary]||'—';});}
  async function refreshSummary(){const buttons=[document.getElementById('refreshDashboard'),document.getElementById('refreshSystem')];buttons.forEach((button)=>button.disabled=true);if(dashboardResult)dashboardResult.textContent='กำลังโหลดสถานะจริงจาก SmartLock…';if(systemResult)systemResult.textContent='กำลังโหลดข้อมูลระบบ…';try{const data=await request('/api/manage/summary',{token});renderSummary(data);let networkLoaded=false;try{const network=await networkRequest('/api/network/status');const modeLabels={STA:'Wi-Fi บ้าน',AP:'จุดกระจายสัญญาณ',AP_STA:'จุดกระจายสัญญาณและ Wi-Fi'};document.getElementById('dashboardNetworkMode').textContent=modeLabels[network.mode]||networkValue(network.mode);document.getElementById('dashboardIp').textContent=networkValue(network.staIp&&network.staIp!=='0.0.0.0'?network.staIp:network.apIp);networkLoaded=true;}catch(_){}if(!token)return;const message=networkLoaded?'อัปเดตข้อมูลล่าสุดแล้ว':'อัปเดตสถานะระบบแล้ว แต่โหลดข้อมูลเครือข่ายไม่ได้';if(dashboardResult){dashboardResult.textContent=message;dashboardResult.className=networkLoaded?'ok':'note';}if(systemResult)systemResult.textContent='อัปเดตข้อมูลระบบล่าสุดแล้ว';}catch(_){if(token){const message='โหลดข้อมูลสถานะไม่ได้ ตรวจสอบการเชื่อมต่อแล้วลองอีกครั้ง';if(dashboardResult){dashboardResult.textContent=message;dashboardResult.className='error';}if(systemResult){systemResult.textContent=message;systemResult.className='error';}}}finally{buttons.forEach((button)=>button.disabled=false);}}
  function switchManagementPage(){if(!token)return;let page=window.location.hash.slice(1);if(page===''){page='dashboard';window.history.replaceState(null,'',window.location.pathname+window.location.search+'#dashboard');}if(!managementPages[page]||(ownerPages.includes(page)&&actorRole!=='Owner'))page='dashboard';Object.keys(managementPages).forEach((key)=>{managementPages[key].hidden=key!==page;});document.querySelectorAll('[data-page-link]').forEach((link)=>{if(link.dataset.pageLink===page)link.setAttribute('aria-current','page');else link.removeAttribute('aria-current');});if(page==='network'){refreshNetworkStatus().catch(()=>showNetworkResult('โหลดสถานะเครือข่ายไม่ได้ ตรวจสอบการเชื่อมต่อแล้วลองอีกครั้ง','error'));}else if(page==='line'){refreshLineStatus();}else if(page==='unlock'){refreshUnlockDuration();}else if(page==='dashboard'||page==='system'){refreshSummary();}}
  function stopNetworkPoll(){if(networkPollTimer){clearTimeout(networkPollTimer);networkPollTimer=0;}}
  function scheduleNetworkStatusPoll(){if(!token)return;stopNetworkPoll();networkPollTimer=setTimeout(async()=>{networkPollTimer=0;try{const data=await refreshNetworkStatus(),state=String(data.staState||'').toLowerCase();if(state==='connecting'){showNetworkResult('กำลังทดสอบการเชื่อมต่อ Wi-Fi…','note');scheduleNetworkStatusPoll();}else if(state==='connected'&&data.staIp){showNetworkResult('เชื่อมต่อแล้ว IP ใน LAN: '+data.staIp,'ok');}else if(state==='failed'){showNetworkResult('เชื่อมต่อ Wi-Fi นี้ไม่ได้ ตรวจสอบรหัสผ่านแล้วลองอีกครั้ง','error');}}catch(_){showNetworkResult('ตรวจสอบสถานะการเชื่อมต่อไม่ได้ กำลังลองอีกครั้ง…','error');scheduleNetworkStatusPoll();}},1500);}
  function renderNetworks(networks){const list=document.getElementById('networkList');list.replaceChildren();selectedNetwork='';if(!Array.isArray(networks)||networks.length===0){const empty=document.createElement('p');empty.className='muted';empty.textContent='ไม่พบเครือข่าย ลองค้นหาอีกครั้งหรือขยับเข้าใกล้เราเตอร์';list.appendChild(empty);return;}networks.forEach((network,index)=>{if(!network||typeof network.ssid!=='string'||!network.ssid)return;const label=document.createElement('label');label.className='networkChoice';const radio=document.createElement('input');radio.type='radio';radio.name='networkSsid';radio.value=network.ssid;radio.addEventListener('change',()=>{selectedNetwork=network.ssid;});const name=document.createElement('span');name.textContent=network.ssid+(network.secure?' · มีการรักษาความปลอดภัย':' · ไม่มีรหัสผ่าน');const signal=document.createElement('small');signal.className='muted';signal.textContent=Number.isFinite(Number(network.rssi))?String(network.rssi)+' dBm':'';label.append(radio,name,signal);list.appendChild(label);if(index===0){radio.checked=true;selectedNetwork=network.ssid;}});}
  async function scanNetworks(){const button=document.getElementById('scanNetworks');button.disabled=true;stopNetworkPoll();showNetworkResult('กำลังค้นหาเครือข่าย Wi-Fi…','note');document.getElementById('networkList').replaceChildren();try{let data=await networkRequest('/api/network/scan');while(data.state==='scanning'){await new Promise((resolve)=>setTimeout(resolve,1000));data=await networkRequest('/api/network/scan');}if(data.state!=='complete'||!Array.isArray(data.networks))throw new Error('scan');renderNetworks(data.networks);showNetworkResult(data.networks.length?'เลือกเครือข่ายและกรอกรหัสผ่านหากจำเป็น':'ค้นหาเสร็จแล้ว แต่ไม่พบเครือข่าย','note');}catch(_){showNetworkResult('ค้นหาเครือข่าย Wi-Fi ไม่ได้ ตรวจสอบการเชื่อมต่อแล้วลองอีกครั้ง','error');}finally{button.disabled=false;}}
  document.getElementById('adminPinForm').addEventListener('submit',async(e)=>{
    e.preventDefault();const f=e.currentTarget,o=document.getElementById('adminPinResult');
    const data={token,current:f.elements.current.value,next:f.elements.next.value,confirm:f.elements.confirm.value};
    f.reset();o.className='note';
    try{
      if(!/^[0-9]{4}$/.test(data.current)){o.textContent='รหัส Admin PIN ปัจจุบันไม่ถูกต้อง';o.className='error';return;}
      if(!/^[0-9]{4}$/.test(data.next)){o.textContent='รหัสใหม่ต้องเป็นตัวเลข 4 หลัก';o.className='error';return;}
      if(data.next!==data.confirm){o.textContent='รหัสใหม่ไม่ตรงกัน';o.className='error';return;}
      o.textContent='กำลังตรวจสอบรหัส…';
      const r=await request('/api/system/admin-pin',data);if(!r.changed)throw Error();o.textContent='เปลี่ยนรหัส Admin PIN สำเร็จ';o.className='ok';
    }catch(error){o.className='error';o.textContent=({current_pin_rejected:'รหัส Admin PIN ปัจจุบันไม่ถูกต้อง',invalid_new_pin:'รหัสใหม่ต้องเป็นตัวเลข 4 หลัก',pin_mismatch:'รหัสใหม่ไม่ตรงกัน',pin_locked:'กรอกรหัสผิดหลายครั้ง กรุณารอ 60 วินาทีแล้วลองใหม่',rate_limited:'ลองหลายครั้งเกินไป กรุณารอ 60 วินาทีแล้วลองใหม่',unlock_active:'รอให้กลอนล็อกกลับก่อนแล้วลองใหม่',owner_required:'เฉพาะเจ้าของเท่านั้นที่เปลี่ยนรหัสได้'})[error&&error.code]||'เปลี่ยนรหัสไม่ได้ ลองใหม่อีกครั้ง';}
    finally{data.current=data.next=data.confirm='';}
  });
  window.addEventListener('hashchange',switchManagementPage);
  async function refreshUnlockDuration(){const out=document.getElementById('unlockResult');try{const d=await request('/api/manage/unlock-duration',{token});renderUnlockDuration(d);}catch(_){out.className='error';out.textContent='โหลดระยะเวลาปลดล็อกไม่ได้ ลองใหม่อีกครั้ง';}}
  function renderUnlockDuration(d){if(!d||!Number.isInteger(d.unlockSeconds))throw new Error('unlock');document.getElementById('unlockCurrent').textContent=String(d.unlockSeconds);const input=document.getElementById('unlockSeconds');input.value=String(d.unlockSeconds);if(Number.isInteger(d.min))input.min=String(d.min);if(Number.isInteger(d.max))input.max=String(d.max);}
  document.getElementById('unlockForm').addEventListener('submit',async(e)=>{
    e.preventDefault();const f=e.currentTarget,o=document.getElementById('unlockResult'),button=f.querySelector('button');
    const seconds=Number(f.elements.seconds.value);
    if(!Number.isInteger(seconds)||seconds<1||seconds>60){o.className='error';o.textContent='กรุณาระบุเป็นจำนวนเต็ม 1–60 วินาที';return;}
    button.disabled=true;o.className='note';o.textContent='กำลังบันทึก…';
    try{const d=await request('/api/manage/unlock-duration',{token,seconds:String(seconds)});renderUnlockDuration(d);o.className='ok';o.textContent='บันทึกระยะเวลาปลดล็อก '+d.unlockSeconds+' วินาทีแล้ว';}
    catch(error){o.className='error';o.textContent=({invalid_duration:'กรุณาระบุเป็นจำนวนเต็ม 1–60 วินาที',unlock_active:'รอให้กลอนล็อกกลับก่อนแล้วลองใหม่',owner_required:'เฉพาะเจ้าของเท่านั้นที่เปลี่ยนค่านี้ได้',storage_unavailable:'บันทึกไม่ได้ ที่เก็บข้อมูลไม่พร้อม'})[error&&error.code]||'บันทึกไม่ได้ ลองใหม่อีกครั้ง';}
    finally{button.disabled=false;}
  });
  document.getElementById('refreshDashboard').addEventListener('click',refreshSummary);
  document.getElementById('refreshSystem').addEventListener('click',refreshSummary);
  document.getElementById('lineRefresh').addEventListener('click',refreshLineStatus);
  document.getElementById('scanNetworks').addEventListener('click',scanNetworks);

  document.getElementById('networkConnectForm').addEventListener('submit',async(event)=>{event.preventDefault();if(!selectedNetwork){showNetworkResult('ค้นหาและเลือกเครือข่าย Wi-Fi ก่อน','error');return;}const passwordInput=document.getElementById('networkPassword'),password=passwordInput.value,button=document.getElementById('networkConnect');button.disabled=true;passwordInput.value='';stopNetworkPoll();showNetworkResult('กำลังเริ่มทดสอบการเชื่อมต่อ Wi-Fi…','note');try{const data=await networkRequest('/api/network/connect',{ssid:selectedNetwork,password});if(data.ok!==true||String(data.state||'').toLowerCase()!=='connecting')throw new Error('connect');showNetworkResult('กำลังทดสอบการเชื่อมต่อ Wi-Fi…','note');scheduleNetworkStatusPoll();}catch(_){showNetworkResult('เริ่มเชื่อมต่อ Wi-Fi ไม่ได้ ตรวจสอบเครือข่ายแล้วลองอีกครั้ง','error');}finally{button.disabled=false;}});
  async function createEnrollment(identity){document.getElementById('enrollResult').hidden=true;if(!lanEnrollment){show('เชื่อมต่อโทรศัพท์กับ Wi-Fi บ้านเครือข่ายเดียวกับ SmartLock ก่อนสร้าง QR ลงทะเบียน','error');return false;}try{const result=await request('/api/manage/enroll',{token,name:identity.name,role:identity.role});showEnrollment(result,identity);show('สร้าง QR ลงทะเบียนแล้ว ให้สแกนด้วยโทรศัพท์ที่เชื่อมต่อ Wi-Fi บ้านเดียวกันภายใน 2 นาที','ok');return true;}catch(error){if(error.code==='name_taken'){show('ชื่อนี้ถูกใช้งานแล้ว กรุณาใช้ชื่ออื่น','error');return false;}show('สร้าง QR ลงทะเบียนไม่สำเร็จ กรุณาลองใหม่หรือเปิดหน้าจัดการใหม่','error');return false;}}
  async function refresh(){try{const data=await request('/api/manage/state',{token});if(!validateState(data))throw new Error('state');actorRole=data.actorRole;lanEnrollment=data.lanEnrollment;document.getElementById('lineNav').hidden=actorRole!=='Owner';document.getElementById('unlockNav').hidden=actorRole!=='Owner';document.getElementById('adminPinNav').hidden=actorRole!=='Owner';configureRoleOptions();const instruction=document.getElementById('enrollInstruction');instruction.hidden=lanEnrollment;instruction.textContent='สร้าง QR ลงทะเบียนได้เมื่อโทรศัพท์เชื่อมต่อ Wi-Fi บ้านเครือข่ายเดียวกับ SmartLock';document.querySelector('#identityForm button').disabled=!lanEnrollment;const list=document.getElementById('identities');list.replaceChildren();if(data.identities.length===0)list.appendChild(card('ยังไม่มีอุปกรณ์ที่ได้รับสิทธิ์','แตะ “+ เพิ่มอุปกรณ์” เพื่อสร้าง QR ลงทะเบียน'));data.identities.forEach((identity)=>{if(!identity||typeof identity.id!=='string'||typeof identity.name!=='string')return;const active=identity.status==='Active',protectedIdentity=identity.id==='D000001',adminLimited=actorRole==='Admin'&&(protectedIdentity||identity.role==='Admin'),entry=document.createElement('article');entry.className='card';const title=document.createElement('strong');title.textContent=identity.name;entry.appendChild(title);const detail=document.createElement('p');detail.className='muted';detail.textContent=identity.id+' · '+(roleLabels[identity.role]||identity.role)+' · '+(active?'ใช้งานอยู่':'เพิกถอนแล้ว');entry.appendChild(detail);if(active&&!adminLimited&&!protectedIdentity){const revoke=document.createElement('button');revoke.type='button';revoke.className='secondary';revoke.textContent='เพิกถอน';revoke.addEventListener('click',async()=>{if(!await confirmInline('ยืนยันการเพิกถอน','ต้องการเพิกถอนสิทธิ์ของ '+identity.name+' หรือไม่'))return;revoke.disabled=true;try{await request('/api/manage/revoke',{token,deviceId:identity.id});show('เพิกถอนสิทธิ์แล้ว','ok');await refresh();}catch(_){show('เพิกถอนสิทธิ์ไม่ได้ ตรวจสอบการเชื่อมต่อแล้วลองอีกครั้ง','error');revoke.disabled=false;}});entry.appendChild(revoke);}list.appendChild(entry);});}catch(_){show('โหลดข้อมูลการจัดการไม่ได้ ลงชื่อเข้าใช้อีกครั้งหรือตรวจสอบการเชื่อมต่อ','error');}}
  let saved=null;try{saved=JSON.parse(localStorage.getItem('smartlock.setup.v1')||'null');}catch(_){}
  const managementSession=new URLSearchParams(window.location.search).get('session')||'';
  if(!managementSession){unavailableMessage('สแกน QR สำหรับจัดการที่แสดงอยู่บนหน้าจอล็อก');return;}
  let credentials=[];
  if(saved&&saved.state==='active'&&typeof saved.credential==='string')credentials.push({id:'D000001',credential:saved.credential});
  try{const items=JSON.parse(localStorage.getItem('smartlock.devices.v1')||'[]');if(Array.isArray(items))credentials.push(...items.filter((item)=>item&&typeof item.id==='string'&&typeof item.credential==='string'));}catch(_){}
  if(!credentials.length){unavailableMessage('เบราว์เซอร์นี้ไม่มีข้อมูลยืนยันของอุปกรณ์ที่ลงทะเบียนไว้ เปิดชื่อ SmartLock หลักในเบราว์เซอร์เครื่องเดิมที่ใช้ตั้งค่า หากจำเป็น ให้กู้คืนผ่าน IP จากหน้าจัดการที่ยืนยันตัวตนแล้วเท่านั้น');return;}
  show('กำลังเชื่อมต่อกับ SmartLock...','note');
  (async()=>{for(const device of credentials){try{const result=await request('/api/manage/login',{session:managementSession,deviceId:device.id,credential:device.credential});if(typeof result.token!=='string')continue;token=result.token;authenticatedDevice=device;app.hidden=false;await refresh();switchManagementPage();return;}catch(_){}}unavailableMessage('ยืนยันตัวตนเพื่อจัดการไม่สำเร็จ ใช้เบราว์เซอร์ของเจ้าของหรือผู้ดูแลที่ยังใช้งานได้ แล้วสแกน QR สำหรับจัดการใหม่');})();
  document.getElementById('toggleIdentityForm').addEventListener('click',(event)=>{const form=document.getElementById('identityForm');form.hidden=!form.hidden;event.currentTarget.setAttribute('aria-expanded',String(!form.hidden));if(!form.hidden)identityNameInput.focus();});
  document.getElementById('identityForm').addEventListener('submit',async(event)=>{event.preventDefault();const form=event.currentTarget;const name=identityNameInput.value.trim(),role=document.getElementById('identityRole').value;if(!name||bytesUtf8(name)<1||bytesUtf8(name)>40){show('ชื่อยาวเกินไป กรุณาใช้ชื่อไม่เกิน 40 ไบต์ UTF-8','error');return;}if(!lanEnrollment){show('เชื่อมต่อโทรศัพท์กับ Wi-Fi บ้านเครือข่ายเดียวกับ SmartLock ก่อนสร้าง QR','error');return;}if(!allowedRoles().includes(role)){show('สิทธิ์นี้สร้างจากบัญชีปัจจุบันไม่ได้','error');return;}const button=form.querySelector('button');button.disabled=true;const identity={name,role};try{const created=await request('/api/manage/enroll',{token,name,role});form.reset();form.hidden=true;document.getElementById('toggleIdentityForm').setAttribute('aria-expanded','false');showEnrollment(created,identity);show('สร้าง QR ลงทะเบียนแล้ว ให้สแกนด้วยโทรศัพท์เครื่องใหม่ภายใน 2 นาที','ok');}catch(error){show(error.code==='name_taken'?'ชื่อนี้ถูกใช้งานแล้ว กรุณาใช้ชื่ออื่น':'สร้าง QR ลงทะเบียนไม่สำเร็จ กรุณาลองใหม่หรือเปิดหน้าจัดการใหม่','error');}finally{button.disabled=!lanEnrollment;}});
})();
</script></body></html>)HTML";

static const char kEnroll[] PROGMEM = R"HTML(<!doctype html>
<html lang="th"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="color-scheme" content="light"><title>ลงทะเบียนอุปกรณ์ · SmartLock</title>
<style>:root{font:16px/1.45 system-ui,-apple-system,"Segoe UI",sans-serif;color:#17212b;background:#f3f6f8}*{box-sizing:border-box}body{margin:0;padding:1rem;min-height:100vh;display:grid;place-items:center}main{width:min(100%,34rem);background:#fff;border:1px solid #dbe3e8;border-radius:1rem;padding:clamp(1.1rem,5vw,2rem);box-shadow:0 8px 30px #102a3a12}h1{font-size:1.55rem;margin:0 0 .45rem}p{color:#455564}button{font:inherit;font-weight:650;min-height:2.9rem;padding:.65rem 1rem;border:0;border-radius:.55rem;background:#075985;color:#fff;cursor:pointer;width:100%;margin-top:1.25rem}button:disabled{opacity:.6;cursor:wait}#status{padding:.75rem;border-radius:.5rem;white-space:pre-wrap}#status:empty{display:none}.error{background:#fff0ed;color:#842b1c}.ok{background:#eaf7ee;color:#17552d}.note{background:#eef6fc;color:#174a70}[hidden]{display:none!important}</style></head><body><main>
<h1>ลงทะเบียนอุปกรณ์นี้</h1><p id="intro">กำลังตรวจสอบลิงก์ลงทะเบียน…</p><button id="submitButton" type="button" hidden>ลงทะเบียน</button>
<p id="status" role="status" aria-live="polite"></p><p><a href="/">กลับไปหน้าหลักของอุปกรณ์</a></p>
</main><script>
(async () => {'use strict';
  const session=new URLSearchParams(window.location.search).get('session')||'',button=document.getElementById('submitButton'),status=document.getElementById('status'),intro=document.getElementById('intro');
  const storageKey='smartlock.devices.v1',pendingKey='smartlock.enroll.pending.v1';let identity=null,expiryDeadline=0,expiryTimer=0,completed=false;
  function updateExpiry(){if(!identity||completed)return;const remaining=Math.max(0,Math.ceil((expiryDeadline-Date.now())/1000));intro.textContent='ลงทะเบียน '+identity.name+' · QR หมดอายุใน '+remaining+' วินาที';if(!remaining){button.hidden=true;clearInterval(expiryTimer);show('QR หมดอายุแล้ว ขอให้ผู้ดูแลสร้าง QR ใหม่','error');}}
  window.addEventListener('focus',updateExpiry);
  document.addEventListener('visibilitychange',updateExpiry);
  function show(message,kind){status.textContent=message;status.className=kind||'note';}
  function randomHex(byteCount){const bytes=new Uint8Array(byteCount);crypto.getRandomValues(bytes);return Array.from(bytes,(byte)=>byte.toString(16).padStart(2,'0')).join('');}
  function readPending(){try{const value=JSON.parse(localStorage.getItem(pendingKey)||'null');return value&&value.state==='pending'&&/^[0-9a-f]{64}$/i.test(value.credential||'')?value:null;}catch(_){return null;}}
  function postComplete(credential){const body=new URLSearchParams({session,credential});return fetch('/api/enroll/complete',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body.toString()});}
  function postReconcile(credential){const body=new URLSearchParams({kind:'enrollment',credential});return fetch('/api/registration/reconcile',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},cache:'no-store',body:body.toString()});}
  function promoteEnrollment(pending,result){
    if(!result||result.ok!==true||!/^D[0-9]{6}$/.test(result.deviceId||'')||result.deviceId==='D000001'||!['Admin','User','Guest'].includes(result.role)||(pending.role&&pending.role!==result.role))return false;
    localStorage.setItem(storageKey,JSON.stringify([{id:result.deviceId,credential:pending.credential,name:pending.name}]));
    localStorage.removeItem(pendingKey);
    completed=true;clearInterval(expiryTimer);
    button.hidden=true;
    show('ลงทะเบียน '+(pending.name||identity&&identity.name||'อุปกรณ์')+' แล้ว ข้อมูลยืนยันจะเก็บไว้ในเบราว์เซอร์นี้','ok');
    return true;
  }
  async function reconcilePending(pending){if(!pending)return 'none';try{const response=await postReconcile(pending.credential);if(response.status===403)return 'not_found';if(!response.ok)return 'unavailable';return promoteEnrollment(pending,await response.json())?'recovered':'invalid';}catch(_){return 'unavailable';}}
  let pendingAtLoad=readPending();
  const pendingRecoveryState=pendingAtLoad?await reconcilePending(pendingAtLoad):'none';
  if(pendingRecoveryState==='recovered')return;
  if(!session){show('ต้องเปิดหน้านี้จาก QR ลงทะเบียนที่เจ้าของ SmartLock สร้างให้','error');return;}
  function candidateCredentials(){
    const candidates=[];
    function add(id,credential){if(/^D[0-9]{6}$/.test(id||'')&&/^[0-9a-f]{64}$/i.test(credential||'')&&!candidates.some(x=>x.id===id&&x.credential===credential))candidates.push({id,credential});}
    try{const saved=JSON.parse(localStorage.getItem(storageKey)||'[]');if(Array.isArray(saved))saved.forEach(x=>{if(x)add(x.id,x.credential);});}catch(_){}
    try{const setup=JSON.parse(localStorage.getItem('smartlock.setup.v1')||'null');if(setup&&setup.state==='active')add('D000001',setup.credential);}catch(_){}
    return candidates;
  }
  async function currentlyRegistered(){
    for(const candidate of candidateCredentials()){
      const body=new URLSearchParams({session,deviceId:candidate.id,credential:candidate.credential});
      const response=await fetch('/api/enroll/credential-status',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},cache:'no-store',body:body.toString()});
      if(!response.ok)throw new Error('status');const result=await response.json();
      if(!result||!['ACTIVE','REVOKED','UNKNOWN'].includes(result.state))throw new Error('status');
      if(result.state==='ACTIVE')return true;
    }
    return false;
  }

  if(!window.crypto||typeof crypto.getRandomValues!=='function'){show('เบราว์เซอร์นี้ไม่รองรับการสร้างข้อมูลยืนยันที่ปลอดภัย','error');return;}
  const checkedAt=Date.now();
  fetch('/api/enroll/check?session='+encodeURIComponent(session),{cache:'no-store'}).then((response)=>{if(!response.ok)throw new Error('session');return response.json();}).then(async(result)=>{
    if(!result||result.ok!==true||typeof result.name!=='string'||typeof result.role!=='string'||!Number.isFinite(result.expiresIn))throw new Error('session');
    if(pendingAtLoad&&pendingAtLoad.session!==session&&pendingRecoveryState!=='not_found')throw new Error('pending_recovery_unavailable');
    if(pendingAtLoad&&pendingAtLoad.session===session){pendingAtLoad.name=pendingAtLoad.name||result.name;pendingAtLoad.role=pendingAtLoad.role||result.role;}
    if(await currentlyRegistered()){show('เบราว์เซอร์นี้ลงทะเบียนกับ SmartLock เครื่องนี้แล้ว','note');return;}
    if(pendingAtLoad&&pendingAtLoad.session!==session){
      // A fresh invitation has now been validated and the old credential has
      // definitively failed active-registration reconciliation. Preserve it,
      // then let this authorized invitation create a separate credential.
      const recoveryKey='smartlock.enroll.pending.recovery.v1';
      let older=[];const raw=localStorage.getItem(recoveryKey);if(raw){older=JSON.parse(raw);if(!Array.isArray(older))throw new Error('pending_recovery_storage');}
      older.push(pendingAtLoad);localStorage.setItem(recoveryKey,JSON.stringify(older));
      localStorage.removeItem(pendingKey);pendingAtLoad=null;
    }
    identity={name:result.name,role:result.role};const labels={Owner:'เจ้าของ',Admin:'ผู้ดูแลระบบ',User:'ผู้ใช้',Guest:'ผู้เยี่ยมชม'};
    expiryDeadline=checkedAt+result.expiresIn*1000;button.hidden=false;expiryTimer=setInterval(updateExpiry,1000);updateExpiry();
  }).catch(()=>show('QR ลงทะเบียนไม่ถูกต้องหรือหมดอายุแล้ว ขอให้ผู้ดูแลสร้าง QR ใหม่','error'));
  button.addEventListener('click',async()=>{
    if(!identity||button.disabled||completed)return;updateExpiry();if(Date.now()>=expiryDeadline)return;let pending=readPending();
    if(!pending){try{pending={state:'pending',session,credential:randomHex(32),name:identity.name,role:identity.role};localStorage.setItem(pendingKey,JSON.stringify(pending));}catch(_){show('เบราว์เซอร์นี้บันทึกข้อมูลยืนยันไว้ในเครื่องไม่ได้ เปิดใช้พื้นที่จัดเก็บแล้วลองอีกครั้ง','error');return;}}
    button.disabled=true;show('กำลังลงทะเบียน '+identity.name+'…','note');
    try{
      if(await reconcilePending(pending)==='recovered')return;
      const response=await postComplete(pending.credential);let result=null;try{result=await response.json();}catch(_){}if(!response.ok||!result||result.ok!==true||typeof result.deviceId!=='string'||!result.deviceId)throw new Error('register');
      if(!promoteEnrollment(pending,{ok:true,deviceId:result.deviceId,role:identity.role}))throw new Error('register');
    }catch(_){if(await reconcilePending(pending)==='recovered')return;show('ยังไม่ได้รับการยืนยันการลงทะเบียน บันทึกข้อมูลยืนยันไว้ในเครื่องแล้ว ลองส่งคำขอเดิมอีกครั้ง','error');}finally{button.disabled=false;}
  });
})();
</script></body></html>)HTML";

static const char kAccess[] PROGMEM = R"HTML(<!doctype html>
<html lang="th"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="color-scheme" content="light"><title>ปลดล็อก SmartLock</title>
<style>:root{font:16px/1.45 system-ui,-apple-system,"Segoe UI",sans-serif;color:#17212b;background:#f3f6f8}*{box-sizing:border-box}body{margin:0;padding:1rem;min-height:100vh;display:grid;place-items:center}main{width:min(100%,30rem);background:#fff;border:1px solid #dbe3e8;border-radius:1rem;padding:clamp(1.1rem,5vw,2rem)}h1{font-size:1.55rem;margin:0 0 .45rem}p{color:#455564}#status{padding:.75rem;border-radius:.5rem;white-space:pre-wrap}#status:empty{display:none}.error{background:#fff0ed;color:#842b1c}.ok{background:#eaf7ee;color:#17552d}.note{background:#eef6fc;color:#174a70}</style></head><body><main>
<h1>การเข้าใช้งาน SmartLock</h1><p id="status" role="status" aria-live="polite">กำลังตรวจสอบอุปกรณ์นี้…</p>
</main><script>
(() => {'use strict';
  const status=document.getElementById('status');
  const match=location.pathname.match(/^\/a\/([0-9a-fA-F]{64})$/);
  function show(message,kind){status.textContent=message;status.className=kind||'note';}
  function ownerCredential(){
    try{const setup=JSON.parse(localStorage.getItem('smartlock.setup.v1')||'null');if(setup&&setup.state==='active'&&/^[0-9a-f]{64}$/i.test(setup.credential||''))return {id:'D000001',credential:setup.credential};}catch(_){}
    try{const devices=JSON.parse(localStorage.getItem('smartlock.devices.v1')||'[]');if(Array.isArray(devices)){const item=devices.slice().reverse().find((entry)=>entry&&typeof entry.id==='string'&&/^[0-9a-f]{64}$/i.test(entry.credential||''));if(item)return {id:item.id,credential:item.credential};}}catch(_){}
    return null;
  }
  async function run(){if(!match){show('ลิงก์ QR สำหรับเข้าใช้งานไม่ถูกต้อง สแกน QR ใหม่จากล็อก','error');return;}const owner=ownerCredential();if(!owner){show('เบราว์เซอร์นี้ยังไม่มีข้อมูลยืนยันของอุปกรณ์ที่ลงทะเบียน โปรดเปิด SmartLock ในเบราว์เซอร์เครื่องเดิมที่เคยลงทะเบียน แล้วสแกน QR สำหรับเข้าใช้งานใหม่ หากเข้าไม่ได้ ให้ใช้วิธีกู้คืนจากหน้าจัดการที่ยืนยันตัวตนแล้วเท่านั้น','note');return;}
    show('กำลังขอเข้าใช้งาน…','note');const body=new URLSearchParams({session:match[1],deviceId:owner.id,credential:owner.credential});try{const response=await fetch('/api/access/request',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body.toString()});let result=null;try{result=await response.json();}catch(_){}if(response.status===403){show('ปฏิเสธการเข้าใช้งาน อุปกรณ์อาจถูกเพิกถอนสิทธิ์ หรือ QR หมดอายุแล้ว หากสแกน QR ใหม่แล้วยังเข้าไม่ได้ ให้ติดต่อเจ้าของ','error');return;}if(!response.ok||!result||result.ok!==true||!Number.isFinite(result.unlockSeconds))throw new Error('request');show('อนุญาตให้เข้าใช้งานแล้ว ปลดล็อกเป็นเวลา '+result.unlockSeconds+' วินาที','ok');}catch(_){show('ยังไม่ได้รับการยืนยันการเข้าใช้งาน QR อาจหมดอายุแล้ว สแกน QR สำหรับเข้าใช้งานใหม่แล้วลองอีกครั้ง','error');}}
  run();
})();
</script></body></html>)HTML";

static const char kReset[] PROGMEM = R"HTML(<!doctype html>
<html lang="th"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="color-scheme" content="light"><title>คืนค่าโรงงาน · SmartLock</title>
<style>:root{font:16px/1.45 system-ui,-apple-system,"Segoe UI",sans-serif;color:#17212b;background:#f3f6f8}*{box-sizing:border-box}body{margin:0;padding:1rem;min-height:100vh;display:grid;place-items:center}main{width:min(100%,34rem);background:#fff;border:1px solid #dbe3e8;border-radius:1rem;padding:clamp(1.1rem,5vw,2rem);box-shadow:0 8px 30px #102a3a12}h1{font-size:1.55rem;line-height:1.2;margin:0 0 .45rem;color:#991b1b}p{color:#455564}.warning{padding:.8rem;border-radius:.6rem;background:#fff0ed;color:#842b1c}.confirm{display:flex;align-items:flex-start;gap:.65rem;margin:1rem 0;font-weight:600}.confirm input{width:1.25rem;height:1.25rem;flex:none;margin:.15rem 0 0;accent-color:#b91c1c}button{font:inherit;font-weight:700;min-height:2.9rem;padding:.65rem 1rem;border:0;border-radius:.55rem;background:#b91c1c;color:#fff;cursor:pointer;width:100%;margin-top:.45rem}button:disabled{opacity:.55;cursor:wait}#status{padding:.75rem;border-radius:.5rem;white-space:pre-wrap}#status:empty{display:none}.error{background:#fff0ed;color:#842b1c}.ok{background:#eaf7ee;color:#17552d}.note{background:#eef6fc;color:#174a70}[hidden]{display:none!important}</style></head><body><main>
<h1>คืนค่า SmartLock เป็นค่าโรงงาน</h1>
<p>การดำเนินการนี้จะลบการตั้งค่า SmartLock เจ้าของ เบราว์เซอร์ที่ลงทะเบียน และการตั้งค่าเครือข่ายที่บันทึกไว้ จากนั้นล็อกจะกลับสู่สถานะเริ่มต้นสำหรับตั้งค่า</p>
<p class="warning"><strong>ไม่สามารถยกเลิกการดำเนินการนี้ได้</strong> ดำเนินการต่อเมื่อพร้อมตั้งค่าล็อกใหม่เท่านั้น</p>
<form id="resetForm" hidden>
  <label class="confirm"><input id="confirmReset" type="checkbox" required><span>ฉันเข้าใจว่าการดำเนินการนี้จะลบการตั้งค่า SmartLock และข้อมูลยืนยันการเข้าใช้งานทั้งหมด</span></label>
  <button id="resetButton" type="submit">ลบข้อมูลทั้งหมดและคืนค่าโรงงาน</button>
</form>
<section id="resetConfirmDialog" class="warning" role="dialog" aria-modal="true" aria-labelledby="resetConfirmTitle" hidden><strong id="resetConfirmTitle">ยืนยันการคืนค่าโรงงาน</strong><p>ต้องการลบการตั้งค่า SmartLock ทั้งหมดและคืนล็อกสู่สถานะตั้งค่าจากโรงงานหรือไม่ ไม่สามารถยกเลิกการดำเนินการนี้ได้</p><button id="resetConfirmProceed" type="button">ยืนยันการคืนค่า</button><button id="resetConfirmCancel" type="button">ยกเลิก</button></section><p id="status" role="status" aria-live="polite"></p>
</main><script>
(() => {'use strict';
  const session=new URLSearchParams(window.location.search).get('session')||'';
  const form=document.getElementById('resetForm'),button=document.getElementById('resetButton'),status=document.getElementById('status'),confirmReset=document.getElementById('confirmReset');
  confirmReset.addEventListener('invalid',()=>confirmReset.setCustomValidity('ยืนยันว่าคุณเข้าใจผลของการคืนค่าโรงงานก่อนดำเนินการ'));confirmReset.addEventListener('change',()=>confirmReset.setCustomValidity(''));
  function show(message,kind){status.textContent=message;status.className=kind||'note';}
  function readOwnerCredentials(){
    const found=[];
    try{const devices=JSON.parse(localStorage.getItem('smartlock.devices.v1')||'[]');if(Array.isArray(devices)){devices.slice().reverse().forEach((item)=>{if(item&&typeof item.id==='string'&&item.id.length>0&&item.id.length<=64&&/^[0-9a-f]{64}$/i.test(item.credential||''))found.push({id:item.id,credential:item.credential});});}}catch(_){}
    try{const setup=JSON.parse(localStorage.getItem('smartlock.setup.v1')||'null');if(setup&&setup.state==='active'&&/^[0-9a-f]{64}$/i.test(setup.credential||''))found.push({id:'D000001',credential:setup.credential});}catch(_){}
    return found;
  }
  if(!/^[0-9a-f]{64}$/i.test(session)){show('ลิงก์คืนค่าไม่ถูกต้อง เปิดหน้าคืนค่าจากล็อกอีกครั้งเพื่อสร้าง QR ใหม่','error');return;}
  let credentials=readOwnerCredentials();
  if(!credentials.length){show('เบราว์เซอร์นี้ไม่มีข้อมูลยืนยันของเจ้าของ เปิด QR สำหรับคืนค่าด้วยเบราว์เซอร์ของเจ้าของที่ได้รับอนุญาต','error');return;}
  form.hidden=false;
  form.addEventListener('submit',async(event)=>{
    event.preventDefault();
    if(!form.reportValidity())return;
    const dialog=document.getElementById('resetConfirmDialog');dialog.hidden=false;const confirmationAccepted=await new Promise(resolve=>{const proceed=document.getElementById('resetConfirmProceed'),cancel=document.getElementById('resetConfirmCancel');const done=value=>{dialog.hidden=true;proceed.removeEventListener('click',yes);cancel.removeEventListener('click',no);resolve(value);},yes=()=>done(true),no=()=>done(false);proceed.addEventListener('click',yes);cancel.addEventListener('click',no);cancel.focus();});if(!confirmationAccepted)return;
    button.disabled=true;show('กำลังยืนยันการคืนค่าโรงงาน...','note');
    let confirmed=false;
    for(const owner of credentials){
      try{
        const body=new URLSearchParams({session,deviceId:owner.id,credential:owner.credential});
        const response=await fetch('/api/reset/confirm',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body.toString()});
        let result=null;try{result=await response.json();}catch(_){}
        if(response.ok&&result&&result.ok===true){confirmed=true;break;}
      }catch(_){}
    }
    if(!confirmed){button.disabled=false;show('ยังไม่ได้รับการยืนยันการคืนค่า ตรวจสอบการเชื่อมต่อและดูว่า QR นี้ยังใช้งานได้ แล้วลองอีกครั้ง','error');return;}
    try{for(let index=localStorage.length-1;index>=0;index--){const key=localStorage.key(index);if(key&&key.toLowerCase().startsWith('smartlock.'))localStorage.removeItem(key);}}catch(_){}
    form.hidden=true;
    show('ยืนยันการคืนค่าโรงงานแล้ว เชื่อมต่อโทรศัพท์กับจุดกระจายสัญญาณของ SmartLock จากนั้นสแกน QR สำหรับตั้งค่าใหม่เพื่อลงทะเบียนอีกครั้ง','ok');
  });
})();
</script></body></html>)HTML";

}  // namespace WebAssets
