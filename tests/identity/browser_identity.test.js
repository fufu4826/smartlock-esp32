// Browser-context tests for the single-identity management and phone enrollment UI.
// These execute the production scripts with isolated fake DOM and localStorage; no HTTP or lock is available.
'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const {webcrypto} = require('node:crypto');
const source = fs.readFileSync('src/web/WebAssets.h', 'utf8');
function productionScript(name) {
  const pattern = new RegExp(`static const char ${name}\\[\\] PROGMEM = R"HTML\\(([\\s\\S]*?)\\)HTML";`);
  const html = source.match(pattern)?.[1];
  assert.ok(html, `missing ${name} HTML`);
  return {html, code:html.match(/<script>([\s\S]*?)<\/script>/)?.[1]};
}
const tick = () => new Promise(resolve => setImmediate(resolve));
function storageWithOwner() {
  const storage = new Map();
  storage.set('smartlock.devices.v1', JSON.stringify([{id:'D000001', credential:'a'.repeat(64), name:'เจ้าของเดิม'}]));
  return storage;
}
async function enrollBrowser(deviceId, registered=false) {
  const {html, code} = productionScript('kEnroll');
  assert.doesNotMatch(html, /<input\b/i, 'phone enrollment must not ask for an additional name or secret');
  assert.match(html, />ลงทะเบียน<\/button>/, 'phone action should only be ลงทะเบียน');
  const storage = registered ? storageWithOwner() : new Map(), elements = new Map(), requests = [];
  const el = id => {
    if (!elements.has(id)) elements.set(id, {hidden:id === 'submitButton', value:'', textContent:'', listeners:{},
      addEventListener(event, handler) { this.listeners[event] = handler; }});
    return elements.get(id);
  };
  const session = 'b'.repeat(64);
  const context = {URLSearchParams, Uint8Array, crypto:webcrypto,
    window:{location:{search:'?session='+session}, crypto:webcrypto},
    document:{getElementById:el},
    localStorage:{getItem:key=>storage.get(key)||null, setItem:(key,value)=>storage.set(key,value), removeItem:key=>storage.delete(key)},
    fetch:async (url, options) => {
      if (url.startsWith('/api/enroll/check')) return {ok:true, json:async()=>({ok:true,name:'คุณสมชาย',role:'Guest',expiresIn:120})};
      if(url==='/api/enroll/credential-status')return {ok:true,json:async()=>({state:'ACTIVE'})};
      const fields=Object.fromEntries(new URLSearchParams(options.body));
      requests.push({url,method:options.method,fields});
      return {ok:true,json:async()=>({ok:true,deviceId})};
    }};
  vm.runInNewContext(code, context);
  await tick();
  return {storage,el,requests,session};
}
function makeElement(id='') {
  return {id, hidden:false, disabled:false, value:'', textContent:'', className:'', dataset:{}, children:[], listeners:{},
    addEventListener(event,handler){this.listeners[event]=handler;},
    appendChild(child){this.children.push(child);return child;}, append(...children){this.children.push(...children);},
    replaceChildren(...children){this.children=[...children];}, setAttribute(name,value){this[name]=value;}, removeAttribute(name){delete this[name];},
    setCustomValidity(){}, focus(){}, scrollIntoView(){}, reset(){this.value='';},
    querySelector(selector){return selector==='button'?this.button||makeElement('formButton'):null;}};
}
async function manageBrowser() {
  const {code} = productionScript('kManage');
  const storage=storageWithOwner(), elements=new Map(), calls=[];let success=false;
  const el=id=>{if(!elements.has(id))elements.set(id,makeElement(id));return elements.get(id);};
  const form=el('identityForm');form.button=makeElement('identitySubmit');
  const navLinks=['dashboard','management','network','logs','cloud','system'].map(page=>{const link=makeElement();link.dataset.pageLink=page;return link;});
  const context={URL,URLSearchParams,Uint8Array,TextEncoder,TextDecoder,
    window:{location:{search:'?session='+('c'.repeat(64)),hash:'#management',hostname:'smartlock-0123456789ab.local',pathname:'/manage'},history:{replaceState(){}} ,addEventListener(){}},
    document:{getElementById:el,createElement:tag=>makeElement(tag),querySelector:selector=>selector==='#identityForm button'?form.button:null,
      querySelectorAll:selector=>selector==='[data-page-link]'?navLinks:[]},
    localStorage:{getItem:key=>storage.get(key)||null,setItem:(key,value)=>storage.set(key,value),removeItem:key=>storage.delete(key)},
    fetch:async(url,options)=>{
      const fields=options&&options.body?Object.fromEntries(new URLSearchParams(options.body)):{};calls.push({url,fields});
      if(url==='/api/manage/login')return {ok:true,status:200,json:async()=>({ok:true,token:'manage-token'})};
      if(url==='/api/manage/state')return {ok:true,status:200,json:async()=>({ok:true,actorRole:'Owner',lanEnrollment:true,identities:[{id:'D000001',name:'เจ้าของเดิม',role:'Owner',status:'Active'}]})};
      if(url==='/api/manage/enroll'&&success)return {ok:true,status:200,json:async()=>({ok:true,url:'http://smartlock-0123456789ab.local/enroll?session='+('d'.repeat(64)),expiresIn:120,qrSvg:"<svg xmlns='http://www.w3.org/2000/svg'/>"})};
      if(url==='/api/manage/enroll')return {ok:false,status:409,json:async()=>({ok:false,error:'name_taken'})};
      throw new Error('unexpected request '+url);
    },
    confirm:()=>true,
  };
  context.location=context.window.location;context.location.href='http://smartlock-0123456789ab.local/manage';context.location.origin='http://smartlock-0123456789ab.local';
  context.setInterval=()=>1;context.clearInterval=()=>{};
  vm.runInNewContext(code,context);
  await tick();await tick();
  assert.equal(el('app').hidden,false,'management UI should authenticate with retained Owner credential');
  assert.equal(el('identities').children.length,1,'flat identity list should render exactly one identity');
  assert.equal(el('identities').children[0].children[0].textContent,'เจ้าของเดิม');
  assert.equal(JSON.parse(storage.get('smartlock.devices.v1'))[0].id,'D000001','existing Owner browser credential must remain stored');
  el('identityName').value='ชื่อซ้ำ';el('identityRole').value='Guest';
  await form.listeners.submit({preventDefault(){},currentTarget:form});
  const enrollment=calls.find(call=>call.url==='/api/manage/enroll');
  assert.deepEqual(Object.keys(enrollment.fields).sort(),['name','role','token']);
  assert.deepEqual(enrollment.fields,{token:'manage-token',name:'ชื่อซ้ำ',role:'Guest'});
  assert.equal(el('status').textContent,'ชื่อนี้ถูกใช้งานแล้ว กรุณาใช้ชื่ออื่น');
  success=true;el('enrollResult').hidden=true;
  const event={preventDefault(){},currentTarget:form};const pending=form.listeners.submit(event);
  event.currentTarget=null; // Native DOM clears currentTarget after synchronous dispatch.
  await pending;
  assert.equal(el('enrollResult').hidden,false,'successful response must show QR after DOM event dispatch ends');
  assert.match(el('enrollQr').src,/^data:image\/svg/);
  assert.match(el('status').textContent,/สร้าง QR ลงทะเบียนแล้ว/);
}
(async()=>{
  const first=await enrollBrowser('D000002'),second=await enrollBrowser('D000003');
  for(const browser of [first,second]){
    assert.equal(browser.el('submitButton').hidden,false);
    assert.match(browser.el('intro').textContent,/คุณสมชาย.*ผู้เยี่ยมชม/);
    await browser.el('submitButton').listeners.click();
    assert.equal(browser.requests.length,1);
    assert.equal(browser.requests[0].url,'/api/enroll/complete');
    assert.deepEqual(Object.keys(browser.requests[0].fields).sort(),['credential','session']);
    assert.equal(browser.requests[0].fields.session,browser.session);
    assert.match(browser.requests[0].fields.credential,/^[0-9a-f]{64}$/);
    assert.equal(browser.el('status').textContent,'ลงทะเบียน คุณสมชาย แล้ว ข้อมูลยืนยันจะเก็บไว้ในเบราว์เซอร์นี้');
  }
  const firstDevices=JSON.parse(first.storage.get('smartlock.devices.v1'));
  const secondDevices=JSON.parse(second.storage.get('smartlock.devices.v1'));
  assert.equal(firstDevices.length,1);assert.equal(secondDevices.length,1);
  assert.equal(firstDevices[0].id,'D000002');assert.equal(secondDevices[0].id,'D000003');
  assert.notEqual(firstDevices[0].credential,secondDevices[0].credential,'separate browser contexts must receive independent random credentials');
  assert.equal(first.storage.has('smartlock.enroll.pending.v1'),false);
  const registered=await enrollBrowser('D000004',true);
  assert.equal(registered.el('submitButton').hidden,true);assert.equal(registered.requests.length,0);
  assert.deepEqual(JSON.parse(registered.storage.get('smartlock.devices.v1')),JSON.parse(storageWithOwner().get('smartlock.devices.v1')));
  await manageBrowser();
  console.log('PASS: identity enrollment name/role display, name_taken message, exact completion fields, independent credentials, and retained Owner storage');
})().catch(error=>{console.error(error);process.exitCode=1;});
