// Exercises Phase 13 maintenance controls in the production management script with browser fakes.
'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync('src/web/WebAssets.h','utf8');
const html=source.match(/static const char kManage\[\] PROGMEM = R"HTML\(([\s\S]*?)\)HTML";/)[1];
const code=html.match(/<script>([\s\S]*?)<\/script>/)[1];
function element(id='') {return {id,hidden:id==='app'||id==='unavailable'||id==='ownerMaintenance',disabled:false,value:'',files:[],textContent:'',className:'',dataset:{},children:[],listeners:{},attrs:{},
 addEventListener(name,fn){this.listeners[name]=fn;},removeEventListener(name){delete this.listeners[name];},appendChild(child){this.children.push(child);return child;},append(...items){this.children.push(...items);},replaceChildren(...items){this.children=[...items];},setAttribute(name,value){this.attrs[name]=value;},removeAttribute(name){delete this.attrs[name];},focus(){},click(){this.clicked=true;},remove(){},scrollIntoView(){},reportValidity(){return true;},reset(){this.value='';},querySelector(){return this.button||element('submit');}};}
async function contextFor(actorRole,legacy=false) {
 const storage=new Map(legacy?[['smartlock.setup.v1',JSON.stringify({state:'active',credential:'a'.repeat(64)})]]:[['smartlock.devices.v1',JSON.stringify([{id:'D000001',credential:'a'.repeat(64),name:'Owner'}])]]),els=new Map(),calls=[];
 const ownerCredentialStore=storage.get('smartlock.devices.v1');let otaResultCalls=0;
 const get=id=>{if(!els.has(id))els.set(id,element(id));return els.get(id);};
 get('identityForm').button=element('identitySubmit');
 get('backupPassphrase').value='Correct horse 7!';get('restorePassphrase').value='Backup pass 9!';
 get('restoreFile').files=[{size:3,arrayBuffer:async()=>new Uint8Array([0,15,255]).buffer,slice(){return {arrayBuffer:async()=>new Uint8Array([0,15,255]).buffer};}}];
 get('otaFile').files=[{size:1500,slice(start,end){return {arrayBuffer:async()=>new Uint8Array(end-start).buffer};}}];
 const links=[];
 class BrowserURL extends URL {static createObjectURL(){return 'blob:test';}static revokeObjectURL(){}}
 const ctx={URL:BrowserURL,URLSearchParams,TextEncoder,Uint8Array,Blob,Intl,
   window:{location:{search:'?session='+'c'.repeat(64),hash:'#dashboard',hostname:'smartlock-0123456789ab.local',pathname:'/manage'},history:{replaceState(){}},addEventListener(){}},location:{href:'http://smartlock-0123456789ab.local/manage',hostname:'smartlock-0123456789ab.local'},
   document:{getElementById:get,body:element('body'),createElement:tag=>{const e=element(tag);if(tag==='a')links.push(e);return e;},querySelector:q=>q==='#identityForm button'?get('identitySubmit'):null,querySelectorAll:q=>q==='[data-page-link]'?[]:[]},
   localStorage:{getItem:key=>storage.get(key)||null,setItem:(k,v)=>storage.set(k,v),removeItem:k=>storage.delete(k),get length(){return storage.size;},key:i=>Array.from(storage.keys())[i]},
   setTimeout:fn=>{queueMicrotask(fn);return 1;},clearTimeout(){},
   fetch:async(url,opts)=>{let fields={};if(opts&&opts.body)fields=Object.fromEntries(new URLSearchParams(opts.body));calls.push({url,fields,method:opts&&opts.method});
     if(url==='/api/manage/login')return {ok:true,status:200,json:async()=>({ok:true,token:'management-token'})};
     if(url==='/api/manage/state')return {ok:true,status:200,json:async()=>({ok:true,actorRole,lanEnrollment:true,identities:[{id:'D000001',name:'เจ้าของ',role:'Owner',status:'Active'}]})};
     if(url==='/api/manage/summary')return {ok:true,status:200,json:async()=>({ok:true,firmware:'Single identity checkpoint',databaseHealthy:true})};
     if(url==='/api/network/status')return {ok:true,status:200,json:async()=>({mode:'STA',staIp:'192.168.1.20',staState:'connected'})};
     if(url==='/api/system/backup')return {ok:true,status:200,blob:async()=>new Blob(['encrypted'])};
     if(url==='/api/system/ota/current')return {ok:true,status:200,blob:async()=>new Blob(['current-firmware'],{type:'application/octet-stream'})};
     if(url==='/api/system/restore/validate')return {ok:true,status:200,json:async()=>({ok:true,identities:2,owners:1,restoreReady:true,activated:false})};
     if(url==='/api/system/restore/activate')return {ok:true,status:200,json:async()=>({ok:true,activated:true,rebooting:true})};
     if(url==='/api/system/ota/prepare')return {ok:true,status:200,json:async()=>({ok:true,locked:true,maxBytes:1000000,ready:true,uploadEnabled:true})};
     if(url==='/api/system/ota/start')return {ok:true,status:200,json:async()=>({ok:true,ticket:'ticket-one',receipt:'receipt-one',maxBytes:1000000,chunkBytes:1024})};
     if(url==='/api/system/ota/chunk')return {ok:true,status:200,json:async()=>({ok:true,accepted:Number(fields.offset)+fields.data.length/2})};
     if(url==='/api/system/ota/finish')return {ok:true,status:200,json:async()=>({ok:true,rebooting:true,sha256:'a'.repeat(64)})};
     if(url==='/api/system/ota/result'){otaResultCalls++;if(otaResultCalls===1)throw new Error('transient network failure');return {ok:true,status:200,json:async()=>({ok:true,state:'completed'})};}
     throw new Error('unexpected endpoint '+url);
   }};
 vm.runInNewContext(code,ctx);await new Promise(r=>setImmediate(r));await new Promise(r=>setImmediate(r));
 return {get,calls,links,storage,ownerCredentialStore};
}
(async()=>{
 assert.doesNotMatch(html,/window\.(?:alert|confirm|prompt)\s*\(/,'management page must not use native dialogs');
 const owner=await contextFor('Owner');assert.equal(owner.get('ownerMaintenance').hidden,false);
 await owner.get('downloadCurrentFirmware').listeners.click({currentTarget:owner.get('downloadCurrentFirmware')});
 const currentFirmware=owner.calls.find(x=>x.url==='/api/system/ota/current');assert.equal(currentFirmware.method,'POST');assert.deepEqual(currentFirmware.fields,{token:'management-token'});assert.equal(owner.links.at(-1).download,'smartlock-current.bin');assert.match(owner.get('currentFirmwareResult').textContent,/ดาวน์โหลดเฟิร์มแวร์ปัจจุบันแล้ว/);assert.equal(owner.storage.get('smartlock.devices.v1'),owner.ownerCredentialStore);
 await owner.get('backupForm').listeners.submit({preventDefault(){},currentTarget:owner.get('backupForm')});
 const backup=owner.calls.find(x=>x.url==='/api/system/backup');assert.deepEqual(Object.keys(backup.fields).sort(),['passphrase','token']);assert.equal(backup.fields.passphrase,'Correct horse 7!');assert.equal(owner.get('backupPassphrase').value,'');assert.equal(owner.links.at(-1).download,'smartlock-backup.slbak');
 await owner.get('restoreValidateForm').listeners.submit({preventDefault(){},currentTarget:owner.get('restoreValidateForm')});
 const restore=owner.calls.find(x=>x.url==='/api/system/restore/validate');assert.deepEqual(Object.keys(restore.fields).sort(),['package','passphrase','token']);assert.equal(restore.fields.package,'000fff');assert.equal(owner.get('restorePassphrase').value,'');assert.match(owner.get('restoreResult').textContent,/ตรวจสอบไฟล์สำรองแล้ว/);assert.match(owner.get('restoreResult').textContent,/ระบบยังไม่ได้เปิดใช้ข้อมูล/);assert.equal(owner.get('restoreActivateForm').hidden,false);
 owner.get('restoreActivatePassphrase').value='Activate pass 8!';
 const restoreActivation=owner.get('restoreActivateForm').listeners.submit({preventDefault(){},currentTarget:owner.get('restoreActivateForm')});owner.get('actionDialogConfirm').listeners.click();await restoreActivation;
 const activate=owner.calls.find(x=>x.url==='/api/system/restore/activate');assert.deepEqual(Object.keys(activate.fields).sort(),['package','passphrase','token']);assert.equal(activate.fields.package,'000fff');assert.equal(owner.get('restoreActivatePassphrase').value,'');assert.match(owner.get('restoreResult').textContent,/กำลังรีบูต/);assert.doesNotMatch(owner.get('restoreResult').textContent,/กู้คืนข้อมูลสำเร็จ/);
 await owner.get('prepareOta').listeners.click({currentTarget:owner.get('prepareOta')});
 const ota=owner.calls.find(x=>x.url==='/api/system/ota/prepare');assert.deepEqual(ota.fields,{token:'management-token'});assert.equal(owner.get('otaUploadForm').hidden,false);assert.match(owner.get('otaResult').textContent,/เตรียมพร้อมแล้ว/);
 const otaUpload=owner.get('otaUploadForm').listeners.submit({preventDefault(){},currentTarget:owner.get('otaUploadForm')});owner.get('actionDialogConfirm').listeners.click();await otaUpload;
 const started=owner.calls.find(x=>x.url==='/api/system/ota/start');assert.deepEqual(started.fields,{token:'management-token',deviceId:'D000001',credential:'a'.repeat(64),size:'1500'});
 const chunks=owner.calls.filter(x=>x.url==='/api/system/ota/chunk');assert.equal(chunks.length,2);assert.equal(chunks[0].fields.offset,'0');assert.equal(chunks[0].fields.data.length,2048);assert.equal(chunks[1].fields.offset,'1024');assert.equal(chunks[1].fields.data.length,952);
 assert.deepEqual(owner.calls.find(x=>x.url==='/api/system/ota/finish').fields,{ticket:'ticket-one'});assert.deepEqual(owner.calls.find(x=>x.url==='/api/system/ota/result').fields,{receipt:'receipt-one'});assert.equal(owner.calls.filter(x=>x.url==='/api/system/ota/result').length,2,'transient result fetch failure is retried');assert.match(owner.get('otaResult').textContent,/อัปเดตเฟิร์มแวร์เสร็จสมบูรณ์/);
 assert.match(html,/role=\"dialog\" aria-modal=\"true\"/);
 const legacy=await contextFor('Owner',true);
 await legacy.get('prepareOta').listeners.click({currentTarget:legacy.get('prepareOta')});
 const legacyUpload=legacy.get('otaUploadForm').listeners.submit({preventDefault(){},currentTarget:legacy.get('otaUploadForm')});legacy.get('actionDialogConfirm').listeners.click();await legacyUpload;
 assert.equal(legacy.calls.find(x=>x.url==='/api/system/ota/start').fields.credential,'a'.repeat(64),'OTA reuses the credential that actually authenticated Management, including setup storage');
 const admin=await contextFor('Admin');assert.equal(admin.get('ownerMaintenance').hidden,true);
 console.log('PASS: owner-only backup/restore/current-firmware UI, token-only firmware download, one-request passphrases, binary backup download, two-step restore activation, authenticated chunked OTA, transient result polling retry, and no native dialogs');
})().catch(error=>{console.error(error);process.exitCode=1;});
