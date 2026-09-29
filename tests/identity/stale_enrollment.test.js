const fs=require('fs'),vm=require('vm'),assert=require('assert'),{webcrypto}=require('crypto');
const src=fs.readFileSync('src/web/WebAssets.h','utf8');const html=src.match(/static const char kEnroll\[\] PROGMEM = R"HTML\(([\s\S]*?)\)HTML";/)[1],code=html.match(/<script>([\s\S]*?)<\/script>/)[1];
const tick=()=>new Promise(r=>setImmediate(r));
async function open({state='UNKNOWN',local=true,valid=true,fail=false,statusFail=false}={}){
 const old='a'.repeat(64),storage=new Map(),nodes={},calls=[];
 if(local){storage.set('smartlock.devices.v1',JSON.stringify([{id:'D000002',credential:old}]));storage.set('smartlock.setup.v1',JSON.stringify({state:'active',credential:old}));}
 const initial=JSON.stringify([...storage]);let failComplete=fail;
 const el=id=>nodes[id]||(nodes[id]={hidden:id==='submitButton',disabled:false,handlers:{},addEventListener(k,f){this.handlers[k]=f}});
 const context={URLSearchParams,Uint8Array,crypto:webcrypto,window:{crypto:webcrypto,location:{search:'?session='+'b'.repeat(64)}},document:{getElementById:el},localStorage:{getItem:k=>storage.get(k)||null,setItem:(k,v)=>storage.set(k,v),removeItem:k=>storage.delete(k)},fetch:async(url,opt={})=>{
 assert(!url.includes(old));const fields=Object.fromEntries(new URLSearchParams(opt.body||''));calls.push({url,fields,method:opt.method});
 if(url.startsWith('/api/enroll/check'))return {ok:valid,json:async()=>({ok:true,name:'New Phone',role:'User',expiresIn:60})};
 if(url==='/api/enroll/credential-status'){assert.equal(opt.method,'POST');assert.deepEqual(Object.keys(fields).sort(),['credential','deviceId','session']);return {ok:!statusFail,json:async()=>({state})};}
 assert.equal(url,'/api/enroll/complete');assert.equal(storage.get('smartlock.devices.v1'),local?JSON.parse(initial).find(x=>x[0]==='smartlock.devices.v1')[1]:undefined);return {ok:!failComplete,json:async()=>({ok:!failComplete,deviceId:'D000003'})};}};
 vm.runInNewContext(code,context);await tick();await tick();return {storage,el,calls,initial,setFail:v=>failComplete=v};
}
(async()=>{
 for(const state of ['UNKNOWN','REVOKED']){const b=await open({state,fail:true});assert.equal(b.el('submitButton').hidden,false);assert.equal(JSON.stringify([...b.storage]),b.initial);await b.el('submitButton').handlers.click();assert.equal(JSON.stringify([...b.storage].filter(x=>x[0]!=='smartlock.enroll.pending.v1')),b.initial);const pending=JSON.parse(b.storage.get('smartlock.enroll.pending.v1'));assert.notEqual(pending.credential,'a'.repeat(64));b.setFail(false);await b.el('submitButton').handlers.click();const current=JSON.parse(b.storage.get('smartlock.devices.v1'));assert.equal(current.length,1);assert.equal(current[0].credential,pending.credential);assert.equal(current[0].id,'D000003');assert(!b.storage.has('smartlock.setup.v1'));assert(!b.storage.has('smartlock.enroll.pending.v1'));}
 const empty=await open({local:false});assert.equal(empty.el('submitButton').hidden,false);await empty.el('submitButton').handlers.click();assert.equal(empty.calls.filter(x=>x.url.includes('credential-status')).length,0);
 const active=await open({state:'ACTIVE'});assert.equal(active.el('submitButton').hidden,true);assert.equal(JSON.stringify([...active.storage]),active.initial);assert.equal(active.calls.filter(x=>x.url.includes('/complete')).length,0);
 for(const opts of [{valid:false},{statusFail:true}]){const b=await open(opts);assert.equal(b.el('submitButton').hidden,true);assert.equal(JSON.stringify([...b.storage]),b.initial);}
 assert(!/<input\b/.test(html));assert(!/console\.|Serial\./.test(code));console.log('Stale browser enrollment tests PASS: none/ACTIVE/UNKNOWN/REVOKED, invalid invite/status, retained old storage on failure, replacement only after success');
})().catch(e=>{console.error(e);process.exitCode=1});
