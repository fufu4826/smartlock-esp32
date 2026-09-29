// Runs the production enrollment script in independent browser-storage fakes.
// No HTTP, real identity, ESP32, or lock operation is available to this test.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const {webcrypto} = require('node:crypto');
const source = fs.readFileSync('src/web/WebAssets.h','utf8');
const html = source.match(/static const char kEnroll\[\] PROGMEM = R"HTML\(([\s\S]*?)\)HTML";/)[1];
const code = html.match(/<script>([\s\S]*?)<\/script>/)[1];
async function browser(deviceId, valid=true) {
  const storage = new Map(), elements = new Map(), requests = [];
  const el = id => {
    if (!elements.has(id)) elements.set(id, {hidden:id==='enrollForm',value:'โทรศัพท์สมชาย',textContent:'',
      listeners:{},addEventListener(event,handler){this.listeners[event]=handler;},
      setCustomValidity(){},reportValidity(){return true;}});
    return elements.get(id);
  };
  const context = {URLSearchParams,Uint8Array,TextEncoder,crypto:webcrypto,
    window:{location:{search:'?session='+deviceId.repeat(10)},crypto:webcrypto},
    document:{getElementById:el},
    localStorage:{getItem:key=>storage.get(key)||null,setItem:(key,value)=>storage.set(key,value),removeItem:key=>storage.delete(key)},
    fetch:async (url,options)=>{
      if (!options) return {ok:valid,json:async()=>({ok:valid,userName:'สมชาย',role:'User',expiresIn:120})};
      const fields=Object.fromEntries(new URLSearchParams(options.body)); requests.push(fields);
      return {ok:true,json:async()=>({ok:true,deviceId})};
    }};
  vm.runInNewContext(code,context);
  await new Promise(resolve=>setImmediate(resolve));
  return {storage,el,requests};
}
(async()=>{
  const first=await browser('D000002'),second=await browser('D000003');
  assert.equal(first.el('enrollForm').hidden,false);
  assert.match(first.el('enrollUser').textContent,/สมชาย/);
  for (const b of [first,second]) {
    await b.el('enrollForm').listeners.submit({preventDefault(){}});
    assert.equal(b.el('enrollForm').hidden,true);
    assert.equal(b.requests.length,1);
    assert.deepEqual(Object.keys(b.requests[0]).sort(),['credential','deviceName','session']);
    assert.match(b.requests[0].credential,/^[0-9a-f]{64}$/);
    assert.equal(b.requests[0].deviceName,'โทรศัพท์สมชาย');
    assert.equal(b.storage.has('smartlock.enroll.pending.v1'),false);
  }
  const one=JSON.parse(first.storage.get('smartlock.devices.v1'))[0];
  const two=JSON.parse(second.storage.get('smartlock.devices.v1'))[0];
  assert.equal(one.id,'D000002'); assert.equal(two.id,'D000003');
  assert.notEqual(one.credential,two.credential);
  assert.equal(first.storage.has('smartlock.setup.v1'),false);
  const expired=await browser('D000004',false);
  assert.equal(expired.el('enrollForm').hidden,true);
  assert.equal(expired.requests.length,0);
  const oversized=await browser('D000005');
  oversized.el('deviceName').value='ก'.repeat(14);
  await oversized.el('enrollForm').listeners.submit({preventDefault(){}});
  assert.equal(oversized.requests.length,0);
  console.log('PASS: independent enrollment browser credentials/storage, Thai name, no admin secret, expired grant and UTF-8 byte limit');
})().catch(error=>{console.error(error);process.exitCode=1;});
