const fs=require('fs'),vm=require('vm'),assert=require('assert');
const backend0=()=>fs.readFileSync('src/network/WebServerManager.cpp','utf8');
const source=fs.readFileSync('src/web/WebAssets.h','utf8');
for(const name of ['kSetup','kManage']){
 const start=source.indexOf('const char '+name),end=source.indexOf(')HTML";',start);
 assert(start>=0&&end>start);const page=source.slice(start,end);
 for(const m of page.matchAll(/<script>([\s\S]*?)<\/script>/g))new vm.Script(m[1]);
}
const start=source.indexOf("document.getElementById('adminPinForm').addEventListener");
const end=source.indexOf("window.addEventListener",start);
let handler,sent,reject=false,rejectCode;const output={};
const context={document:{getElementById(id){return id==='adminPinResult'?output:{addEventListener(_,fn){handler=fn}}}},token:'owner-session',request:async(path,data)=>{sent={path,...data};if(reject){const e=Error();e.code=rejectCode;throw e;}return {changed:true}}};
vm.runInNewContext(source.slice(start,end),context);
function form(a,b,c){return {elements:{current:{value:a},next:{value:b},confirm:{value:c}},reportValidity(){return true},reset(){for(const field of Object.values(this.elements))field.value=''}}}
(async()=>{
 let f=form('1234','5678','5678');await handler({preventDefault(){},currentTarget:f});assert(sent.path==='/api/system/admin-pin');assert(sent.current==='1234');assert(sent.next==='5678');assert(output.textContent==='เปลี่ยนรหัส Admin PIN สำเร็จ');assert(f.elements.current.value===''&&f.elements.next.value===''&&f.elements.confirm.value==='');
 sent=null;f=form('1234','5678','5679');await handler({preventDefault(){},currentTarget:f});assert(sent===null);
 reject=true;f=form('1234','5678','5678');await handler({preventDefault(){},currentTarget:f});assert(output.textContent.includes('ไม่ได้'));
 // Clear Thai feedback; client-side format/match errors never reach the server; fields always cleared.
 for(const [code,text] of [['current_pin_rejected','รหัส Admin PIN ปัจจุบันไม่ถูกต้อง'],['pin_locked','60 วินาที'],['invalid_new_pin','ตัวเลข 4 หลัก'],['pin_mismatch','ไม่ตรงกัน']]){rejectCode=code;f=form('1111','5678','5678');await handler({preventDefault(){},currentTarget:f});assert(output.textContent.includes(text),code);assert(f.elements.current.value==='');}
 reject=false;rejectCode=undefined;
 sent=null;f=form('1234','56a8','56a8');await handler({preventDefault(){},currentTarget:f});assert(sent===null&&output.textContent==='รหัสใหม่ต้องเป็นตัวเลข 4 หลัก');
 sent=null;f=form('12','5678','5678');await handler({preventDefault(){},currentTarget:f});assert(sent===null&&output.textContent==='รหัส Admin PIN ปัจจุบันไม่ถูกต้อง');
 sent=null;f=form('1234','5678','5679');await handler({preventDefault(){},currentTarget:f});assert(sent===null&&output.textContent==='รหัสใหม่ไม่ตรงกัน'&&f.elements.next.value==='');
 // Existing default-PIN policy is preserved as-is: the server does not add a new 1234 rule.
 const pinHandler=backend0().slice(backend0().indexOf('void WebServerManager::adminPinChange()'));assert(!pinHandler.slice(0,pinHandler.indexOf('\nvoid WebServerManager::unlockDuration')).includes('"1234"'));
 // Owner-only unlock duration: owner gate before any storage write; single ConfigStore source; never unlocks.
 const ud=backend0().slice(backend0().indexOf('void WebServerManager::unlockDuration()'));const udBody=ud.slice(0,ud.indexOf('\n}\n'));
 const gate=udBody.indexOf('lineOwnerAuthorized()'),save=udBody.indexOf('config_->save(');assert(gate>0&&save>gate);
 assert(udBody.indexOf('validPost(')<gate);assert(udBody.includes('lock_->isLocked()')&&udBody.indexOf('lock_->isLocked()')<save);
 assert(!/lock_->unlock\(|unlock\(/.test(udBody.replace(/unlock_active|remainingUnlockMs|unlockDurationMs|kMinUnlockSeconds|kMaxUnlockSeconds|unlockSeconds/g,'')));
 assert(udBody.includes('next.unlockDurationMs=seconds*1000'));assert(udBody.includes('seconds<kMinUnlockSeconds||seconds>kMaxUnlockSeconds'));
 assert(backend0().includes('server_.on("/api/manage/unlock-duration", HTTP_POST'));assert(!backend0().includes('server_.on("/api/manage/unlock-duration", HTTP_GET'));
 const hdr=fs.readFileSync('src/network/WebServerManager.h','utf8');assert(hdr.includes('kMinUnlockSeconds = 1, kMaxUnlockSeconds = 60'));
 const cfg=fs.readFileSync('src/storage/ConfigStore.cpp','utf8');assert(cfg.includes('v.unlockDurationMs < 1000 || v.unlockDurationMs > 60000'));
 // Management page: Owner-only navigation, merged LINE page with Add Friend, no secrets/browser storage of PIN.
 const manage=source.slice(source.indexOf('const char kManage'),source.indexOf(')HTML";',source.indexOf('const char kManage')));
 const nav=[...manage.matchAll(/data-page-link="([^"]+)"/g)].map(m=>m[1]);assert.deepStrictEqual(nav,['dashboard','management','network','line','unlock','admin-pin','system']);
 assert(manage.includes("const ownerPages=['line','unlock','admin-pin']"));assert(manage.includes("document.getElementById('unlockNav').hidden=actorRole!=='Owner'"));assert(manage.includes("document.getElementById('adminPinNav').hidden=actorRole!=='Owner'"));
 assert(!manage.includes('lineFriendPanel')&&!manage.includes('line-friend'));
 assert(/id="linePanel"[^]*id="lineAddFriend"/.test(manage));assert(manage.includes("addFriend.href='https://line.me/R/ti/p/'+encodeURIComponent(data.publicBasicId)"));
 assert(/id="unlockSeconds"[^>]*inputmode="numeric"[^>]*min="1" max="60"/.test(manage));
 for(const n of ['current','next','confirm'])assert(new RegExp('name="'+n+'" type="password" inputmode="numeric"').test(manage));
 assert(!/(localStorage|sessionStorage|document\.cookie)[^\n]*(pin|current|next)/i.test(manage.slice(manage.indexOf("getElementById('adminPinForm')"))));
 assert(!/[?&](pin|current|next|confirm)=/.test(manage));
 // Unlock form handler behaviour.
 {const us=source.indexOf("document.getElementById('unlockForm').addEventListener"),ue=source.indexOf('\n  });\n',us)+6;let uh,req=[];const res={};const inputEl={value:'',min:'',max:''};const cur={};
  const ctx={document:{getElementById(id){return id==='unlockResult'?res:id==='unlockSeconds'?inputEl:id==='unlockCurrent'?cur:{addEventListener(_,fn){uh=fn}}}},token:'owner-session',Number,String,
   request:async(path,data)=>{req.push({path,...data});return {unlockSeconds:Number(data.seconds),min:1,max:60,saved:true}},
   renderUnlockDuration:(d)=>{cur.textContent=String(d.unlockSeconds);}};
  vm.runInNewContext(source.slice(us,ue),ctx);const btn={};const uf=v=>({elements:{seconds:{value:v}},querySelector(){return btn}});
  await uh({preventDefault(){},currentTarget:uf('15')});assert(req.length===1&&req[0].path==='/api/manage/unlock-duration'&&req[0].seconds==='15'&&req[0].token==='owner-session');assert(res.textContent.includes('15')&&cur.textContent==='15');
  for(const bad of ['0','61','2.5','abc','']){await uh({preventDefault(){},currentTarget:uf(bad)});}assert(req.length===1&&res.textContent.includes('1–60'));}
 assert(source.includes('const {adminPin, pinConfirm, ...publicPending} = payload;'));
 const backend=fs.readFileSync('src/network/WebServerManager.cpp','utf8');const pin=backend.slice(backend.indexOf('void WebServerManager::adminPinChange()'));
 assert(pin.includes('ownerPinAuthorized()'));assert(backend.includes('enrollment_->authorizedOwner(token,millis())'));
 const functionBody=name=>{const begin=backend.indexOf('WebServerManager::'+name+'(');assert(begin>=0,name+' handler exists');const end=backend.indexOf('\nvoid WebServerManager::',begin+1);return backend.slice(begin,end<0?undefined:end)};
 const ordered=(body,gate,operation)=>{const a=body.indexOf(gate),b=body.indexOf(operation);assert(a>=0&&b>a,gate+' must precede '+operation)};
 ordered(functionBody('managementEnroll'),'managementAuthorized()','createEnrollment(');
 ordered(functionBody('managementRevoke'),'managementAuthorized()','enrollment_->revoke(');
 ordered(functionBody('networkConnect'),'managementAuthorized()','startCandidateSta(');
 ordered(functionBody('lineStatus'),'validPost(1)','lineOwnerAuthorized()');
 ordered(functionBody('lineLabel'),'validPost(2)','lineOwnerAuthorized()');
 ordered(functionBody('lineLabel'),'lineOwnerAuthorized()','LineNotifications::rename(');
 ordered(functionBody('lineTest'),'validPost(1)','lineOwnerAuthorized()');
 ordered(functionBody('lineTest'),'lineOwnerAuthorized()','LineNotifications::requestTest()');
 ordered(functionBody('lineDisconnect'),'validPost(1)','lineOwnerAuthorized()');
 ordered(functionBody('lineDisconnect'),'lineOwnerAuthorized()','LineNotifications::disconnect()');
 ordered(functionBody('lineMaintenanceArm'),'validPost(2, 256)','ownerPinAuthorized()');
 ordered(functionBody('lineMaintenanceArm'),'ownerPinAuthorized()','AdminPin::verify(');
 ordered(functionBody('lineMaintenanceCommit'),'validPost(server_.hasArg("userId") ? 6 : 5, 2048)','lineOwnerAuthorized()');
 assert(!functionBody('lineMaintenanceCommit').includes('copyArg("userId"'));assert(functionBody('lineMaintenanceCommit').includes('LineNotifications::save(label, lineToken, basicId)'));
 {const line=fs.readFileSync('src/notifications/LineNotifications.cpp','utf8');assert(!line.includes('message/push'));assert(line.includes('kDeliveryPath = "/v2/bot/message/broadcast"'));assert(!line.split('\n').filter(l=>!l.trim().startsWith('//')).join('\n').includes('\\"to\\"'));}
 assert(source.includes('การแจ้งเตือนจะถูกส่งไปยังเพื่อนทุกคนของบัญชี LINE Official Account นี้'));assert(!source.includes('ที่ตั้งค่าไว้ในอุปกรณ์นี้เท่านั้น'));assert(source.includes('ส่งข้อความทดสอบถึงเพื่อนทุกคน'));
 ordered(functionBody('lineMaintenanceCommit'),'lineOwnerAuthorized()','LineNotifications::save(');
 ordered(functionBody('lineMaintenanceCancel'),'validPost(1, 256)','lineOwnerAuthorized()');
 ordered(functionBody('lineMaintenanceCancel'),'lineOwnerAuthorized()','clearLineMaintenance()');
 ordered(functionBody('adminPinChange'),'ownerPinAuthorized()','AdminPin::change(');
 ordered(functionBody('ownerPinAuthorized'),'managementAuthorized()','authorizedOwner(');
 ordered(functionBody('lineOwnerAuthorized'),'managementAuthorized()','authorizedOwner(');
 assert(!backend.includes('server_.on("/api/line/status", HTTP_GET'));
 assert(!backend.includes('server_.on("/api/line/test", HTTP_GET'));
 assert(!backend.includes('server_.on("/api/line/disconnect", HTTP_GET'));
 assert(!backend.includes('server_.on("/api/line/save", HTTP_POST'));
 for(const route of ['status','label','test','disconnect','maintenance/arm','maintenance/commit','maintenance/cancel'])assert(backend.includes(`server_.on("/api/line/${route}", HTTP_POST`));
 assert(!backend.includes('/api/line/provisioning/arm'));assert(!backend.includes('/api/line/provisioning/cancel'));
 assert(!source.includes('/api/logs/'));
 assert(!source.includes('id="logsPanel"'));
 assert(!source.includes('id="lineToken"'));assert(!source.includes('id="lineUserId"'));
 assert(!source.includes('/api/line/save'));
 assert(!source.includes('/api/line/provisioning/arm'));assert(!source.includes('/api/line/provisioning/cancel'));
 assert(!source.includes('lineCancelArm'));
 const lineScript=source.slice(source.indexOf("document.getElementById('lineArm').addEventListener"),source.indexOf('let networkPollTimer=0,selectedNetwork=',source.indexOf("document.getElementById('lineArm').addEventListener")));
 assert(lineScript.includes("lineMaintenanceForm').hidden=false"));
 assert(lineScript.includes("request('/api/line/maintenance/arm'"));assert(lineScript.includes("request('/api/line/maintenance/cancel'"));
 assert(!source.includes('lineToken')&&!source.includes('userId'));
 assert(lineScript.includes('form.reset()'));
 const lineStatus=functionBody('lineStatus');assert(lineStatus.includes('deviceLabel'));assert(lineStatus.includes('publicBasicId'));assert(lineStatus.includes('configVersion'));assert(!lineStatus.includes('userId'));
 const reconcile=functionBody('reconcileRegistration');
 assert(reconcile.includes('(setup || homeLanRequest())'));
 assert(reconcile.includes('if(!valid){server_.send(403'));
 const files=['src/main.cpp','src/app/PhysicalAdmin.cpp','src/security/AdminPin.cpp'];for(const path of files){const text=fs.readFileSync(path,'utf8');assert(!/Serial\.(?:print|printf|println)[^;]*\b(?:pin_|oldPin|adminPin)\b/.test(text));}
 assert(!/localStorage[^\n]*adminPin/.test(source));
 console.log('Admin web/Setup focused checks PASS; both changed page scripts syntax PASS');
})().catch(e=>{console.error(e);process.exitCode=1});
