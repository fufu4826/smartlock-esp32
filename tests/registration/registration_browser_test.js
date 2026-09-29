// Runs production Setup, Enrollment, and Access scripts in isolated browser fakes.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const {webcrypto} = require('node:crypto');
const source = fs.readFileSync('src/web/WebAssets.h', 'utf8');
function productionScript(name) {
  const html = source.match(new RegExp(`static const char k${name}\\[\\] PROGMEM = R"HTML\\(([\\s\\S]*?)\\)HTML";`))[1];
  return html.match(/<script>([\s\S]*?)<\/script>/)[1];
}
const scripts = Object.fromEntries(['Setup','Enroll','Access'].map(name => [name, productionScript(name)]));
const turn = () => new Promise(resolve => setImmediate(resolve));
function element(hidden = false) {
  return {hidden, value:'', textContent:'', className:'', disabled:false, listeners:{},
    addEventListener(type, handler) { this.listeners[type] = handler; },
    setCustomValidity(){}, reportValidity(){return true;}, reset(){},
    scrollIntoView(){}, focus(){}};
}
function elements(hiddenIds = []) {
  const map = new Map();
  return id => { if (!map.has(id)) map.set(id, element(id === 'setupForm' || id === 'enrollForm' || id === 'networkDetails' || id === 'pendingNetworkDetails' || hiddenIds.includes(id))); return map.get(id); };
}
function contextFor(storage, getElement, fetch, location) {
  return {URL,URLSearchParams,TextEncoder,Uint8Array,crypto:webcrypto,Date,setInterval:()=>1,clearInterval(){},
    window:{crypto:webcrypto,location,addEventListener(){}},location,document:{getElementById:getElement,visibilityState:'visible',addEventListener(){}},
    localStorage:{getItem:key=>storage.get(key)||null,setItem:(key,value)=>storage.set(key,value),removeItem:key=>storage.delete(key),
      get length(){return storage.size;},key:index=>Array.from(storage.keys())[index]},fetch};
}
function runScript(code, context) { vm.runInNewContext(code, context); }

async function setupReconciliation() {
  const oldCredential='a'.repeat(64), newCredential='b'.repeat(64);
  const payload={session:'c'.repeat(64),ownerName:'ko',unlockSeconds:'5',credential:newCredential,apPassword:'f'.repeat(32)};
  const storage=new Map([
    ['smartlock.devices.v1',JSON.stringify([{id:'D000002',credential:oldCredential,name:'old'}])],
    ['smartlock.setup.v1',JSON.stringify({state:'active',session:'old',credential:oldCredential})],
    ['smartlock.setup.pending.v1',JSON.stringify({state:'pending',payload})]
  ]);
  const get=elements(['submitButton']), calls=[];
  const fetch=async(url,options)=>{calls.push({url,options});if(url==='/api/status')return {ok:false};
    if(url==='/api/registration/reconcile')return {ok:true,json:async()=>({ok:true,deviceId:'D000001',role:'Owner',apSsid:'setup'})};
    throw new Error('unexpected request');};
  runScript(scripts.Setup,contextFor(storage,get,fetch,{search:'',pathname:'/setup',hostname:'smartlock-04225a0ff0a4.local'}));
  await turn(); await turn();
  assert.equal(storage.has('smartlock.setup.pending.v1'),false,'verified reconciliation clears pending key');
  assert.equal(JSON.parse(storage.get('smartlock.setup.v1')).credential,newCredential,'new Owner credential promoted');
  assert.equal(JSON.parse(storage.get('smartlock.devices.v1'))[0].id,'D000001','Owner replaces stale device entry only after proof');
  assert.equal(calls.filter(call=>call.url==='/api/setup/complete').length,0,'consumed Setup is recovered without replaying it');

  const accessGet=elements(), accessCalls=[];
  const accessContext=contextFor(storage,accessGet,async(url,options)=>{accessCalls.push({url,options});return {ok:true,status:200,json:async()=>({ok:true,unlockSeconds:5})};},
    {search:'',pathname:'/a/'+('d'.repeat(64)),hostname:'smartlock-04225a0ff0a4.local'});
  runScript(scripts.Access,accessContext);
  await turn();
  const accessFields=new URLSearchParams(accessCalls[0].options.body);
  assert.equal(accessFields.get('deviceId'),'D000001','Access prioritizes the committed Owner credential');
  assert.equal(accessFields.get('credential'),newCredential);
}

async function failedSetupProofPreservesStorage() {
  const active={state:'active',session:'prior',credential:'1'.repeat(64)};
  const pending={state:'pending',payload:{session:'2'.repeat(64),ownerName:'next',unlockSeconds:'5',credential:'3'.repeat(64),apPassword:'4'.repeat(32)}};
  const storage=new Map([['smartlock.setup.v1',JSON.stringify(active)],['smartlock.setup.pending.v1',JSON.stringify(pending)],
    ['smartlock.devices.v1',JSON.stringify([{id:'D000009',credential:'5'.repeat(64)}])]]);
  const get=elements();
  runScript(scripts.Setup,contextFor(storage,get,async url=>url==='/api/registration/reconcile'?{ok:false}:{ok:false},
    {search:'',pathname:'/setup',hostname:'smartlock-04225a0ff0a4.local'}));
  await turn(); await turn();
  assert.deepEqual(JSON.parse(storage.get('smartlock.setup.v1')),active,'failed proof never replaces existing active credential');
  assert.equal(storage.get('smartlock.setup.pending.v1'),JSON.stringify(pending),'failed proof preserves pending data');
  assert.equal(JSON.parse(storage.get('smartlock.devices.v1'))[0].id,'D000009');
  assert.equal(get('pendingNetworkDetails').hidden,false,'pending recovery exposes credentials if the protected AP restarted');
  assert.equal(get('pendingApPassword').textContent,pending.payload.apPassword,'pending AP secret remains available without marking registration active');
}

async function legacyPendingRecoversOnFirstVisit() {
  const payload={session:'a'.repeat(64),credential:'b'.repeat(64),ownerName:'ko',apPassword:'c'.repeat(32)};
  const storage=new Map([['smartlock.setup.v1',JSON.stringify({state:'pending',payload})]]);
  runScript(scripts.Setup,contextFor(storage,elements(),async url=>url==='/api/registration/reconcile'
    ?{ok:true,json:async()=>({ok:true,deviceId:'D000001',role:'Owner'})}:{ok:false},
    {search:'',pathname:'/setup',hostname:'smartlock-04225a0ff0a4.local'}));
  await turn();await turn();
  assert.equal(JSON.parse(storage.get('smartlock.setup.v1')).state,'active');
  assert.equal(JSON.parse(storage.get('smartlock.setup.v1')).credential,payload.credential);
}

async function setupCommitWithLostResponse() {
  const credential='e'.repeat(64), session='f'.repeat(64);
  const payload={session,ownerName:'new owner',unlockSeconds:'5',credential,apPassword:'1'.repeat(32)};
  const previous={state:'active',session:'old',credential:'2'.repeat(64)};
  const storage=new Map([['smartlock.setup.v1',JSON.stringify(previous)],
    ['smartlock.devices.v1',JSON.stringify([{id:'D000002',credential:'3'.repeat(64)}])],
    ['smartlock.setup.pending.v1',JSON.stringify({state:'pending',payload})]]);
  const get=elements(), calls=[];
  let committed=false, completeCount=0;
  const fetch=async(url,options)=>{
    calls.push({url,options});
    if(url==='/api/status')return {ok:false};
    if(url==='/api/registration/reconcile')return committed && new URLSearchParams(options.body).get('credential')===credential
      ? {ok:true,json:async()=>({ok:true,deviceId:'D000001',role:'Owner',apSsid:'setup'})}
      : {ok:false,status:403,json:async()=>({ok:false})};
    if(url==='/api/setup/complete'){
      completeCount++;
      assert.equal(new URLSearchParams(options.body).get('credential'),credential);
      committed=true;
      throw new Error('successful server commit with lost HTTP response');
    }
    throw new Error('unexpected request '+url);
  };
  runScript(scripts.Setup,contextFor(storage,get,fetch,{search:'?session='+session,pathname:'/setup',hostname:'smartlock-04225a0ff0a4.local'}));
  await turn(); await turn();
  get('adminPin').value='2345';get('pinConfirm').value='2345';
  await get('setupForm').listeners.submit({preventDefault(){}});
  assert.equal(completeCount,1,'one Setup completion created the Owner');
  assert.equal(JSON.parse(storage.get('smartlock.setup.v1')).credential,credential,'the exact committed credential was promoted');
  assert.equal(JSON.parse(storage.get('smartlock.devices.v1')).length,1,'only the one committed Owner remains in device selection');
  assert.equal(JSON.parse(storage.get('smartlock.devices.v1'))[0].id,'D000001');
  assert.equal(storage.has('smartlock.setup.pending.v1'),false,'pending state clears only after proof and promotion');
}

async function setupFreshQrKeepsPendingCredential() {
  const credential='9'.repeat(64), newSession='a'.repeat(64);
  const payload={session:'b'.repeat(64),ownerName:'owner',unlockSeconds:'5',credential,apPassword:'c'.repeat(32)};
  const storage=new Map([['smartlock.setup.pending.v1',JSON.stringify({state:'pending',payload})]]),get=elements();
  let submitted=null;
  const fetch=async(url,options)=>{
    if(url==='/api/status')return {ok:false};
    if(url==='/api/registration/reconcile')return {ok:false,status:403};
    if(url==='/api/setup/complete'){submitted=Object.fromEntries(new URLSearchParams(options.body));return {status:200,json:async()=>({ok:true,apSsid:'setup'})};}
    throw new Error('unexpected request '+url);
  };
  runScript(scripts.Setup,contextFor(storage,get,fetch,{search:'?session='+newSession,pathname:'/setup',hostname:'smartlock-04225a0ff0a4.local'}));
  await turn(); await turn();
  get('adminPin').value='2345';get('pinConfirm').value='2345';
  await get('setupForm').listeners.submit({preventDefault(){}});
  assert.equal(submitted.credential,credential,'new Setup QR retries with the same proof');
  assert.equal(submitted.session,newSession,'only the invitation session changes');
  assert.equal(JSON.parse(storage.get('smartlock.setup.v1')).credential,credential);
}

async function enrollmentLostResponseAndRevisit() {
  const ownerCredential='6'.repeat(64), newCredential='7'.repeat(64), session='8'.repeat(64);
  const setupActive={state:'active',session:'old',credential:ownerCredential,apSsid:'ap',apPassword:'9'.repeat(32)};
  const storage=new Map([['smartlock.setup.v1',JSON.stringify(setupActive)],['smartlock.devices.v1',JSON.stringify([{id:'D000004',credential:'a'.repeat(64)}])]]);
  let committed=false, completes=0, consumed=false, committedCredential='';
  const get=elements(['submitButton']), calls=[];
  const fetch=async(url,options)=>{
    calls.push(url);
    if(url==='/api/registration/reconcile')return committed && new URLSearchParams(options.body).get('credential')===committedCredential
      ? {ok:true,json:async()=>({ok:true,deviceId:'D000005',role:'User'})} : {ok:false,status:403};
    if(url.startsWith('/api/enroll/check'))return {ok:!consumed,json:async()=>({ok:true,name:'hi',role:'User',expiresIn:120})};
    if(url==='/api/enroll/credential-status')return {ok:true,json:async()=>({state:'UNKNOWN'})};
    if(url==='/api/enroll/complete'){completes++;committedCredential=new URLSearchParams(options.body).get('credential');committed=true;consumed=true;throw new Error('lost successful response');}
    throw new Error('unexpected request '+url);
  };
  runScript(scripts.Enroll,contextFor(storage,get,fetch,{search:'?session='+session,pathname:'/enroll',hostname:'smartlock-04225a0ff0a4.local'}));
  await turn(); await turn();
  assert.equal(get('submitButton').hidden,false,'valid invitation displays enrollment action');
  await get('submitButton').listeners.click();
  assert.equal(completes,1,'one completion request was committed');
  assert.equal(consumed,true,'invitation remains consumed');
  const enrolled=JSON.parse(storage.get('smartlock.devices.v1'))[0];
  assert.equal(enrolled.id,'D000005','reconciliation recovers the same committed identity');
  assert.match(enrolled.credential,/^[0-9a-f]{64}$/);
  assert.equal(enrolled.credential,committedCredential);
  assert.equal(storage.get('smartlock.setup.v1'),JSON.stringify(setupActive),'Owner credential remains untouched');

  const revisitStorage=new Map([['smartlock.enroll.pending.v1',JSON.stringify({state:'pending',session,credential:newCredential,name:'hi'})],
    ['smartlock.setup.v1',JSON.stringify(setupActive)]]);
  const revisitGet=elements(['submitButton']), revisitCalls=[];
  runScript(scripts.Enroll,contextFor(revisitStorage,revisitGet,async(url,options)=>{
    revisitCalls.push(url);
    if(url==='/api/registration/reconcile')return {ok:true,json:async()=>({ok:true,deviceId:'D000005',role:'User'})};
    throw new Error('reconciliation must precede consumed invitation check');
  },{search:'',pathname:'/enroll',hostname:'smartlock-04225a0ff0a4.local'}));
  await turn(); await turn();
  assert.equal(JSON.parse(revisitStorage.get('smartlock.devices.v1'))[0].id,'D000005');
  assert.equal(revisitCalls.includes('/api/enroll/check?session='+session),false,'recovery runs before invitation validation and needs no QR');
  assert.equal(revisitStorage.get('smartlock.setup.v1'),JSON.stringify(setupActive));
}

async function rejectedEnrollmentProofPreservesPending() {
  const pending={state:'pending',session:'b'.repeat(64),credential:'c'.repeat(64),name:'revoked'};
  const storage=new Map([['smartlock.enroll.pending.v1',JSON.stringify(pending)]]), get=elements(['submitButton']), calls=[];
  runScript(scripts.Enroll,contextFor(storage,get,async url=>{calls.push(url);return {ok:false,status:403};},
    {search:'',pathname:'/enroll',hostname:'smartlock-04225a0ff0a4.local'}));
  await turn(); await turn();
  assert.equal(storage.get('smartlock.enroll.pending.v1'),JSON.stringify(pending),'wrong/revoked proof is retained for no silent replacement');
  assert.equal(storage.has('smartlock.devices.v1'),false,'rejected proof cannot promote browser credential');
  assert.equal(calls.some(url=>url.startsWith('/api/enroll/check')),false,'no invitation needed to attempt rejected-proof reconciliation');
}

async function freshEnrollmentRotatesOnlyAfterDefinitiveProofFailure() {
  const stale={state:'pending',session:'d'.repeat(64),credential:'e'.repeat(64),name:'old',role:'User'};
  const freshSession='f'.repeat(64),storage=new Map([['smartlock.enroll.pending.v1',JSON.stringify(stale)]]),get=elements(['submitButton']);
  let completeFields=null;
  const fetch=async(url,options)=>{
    if(url==='/api/registration/reconcile')return {ok:false,status:403};
    if(url.startsWith('/api/enroll/check'))return {ok:true,json:async()=>({ok:true,name:'new',role:'Guest',expiresIn:120})};
    if(url==='/api/enroll/complete'){completeFields=Object.fromEntries(new URLSearchParams(options.body));return {ok:true,json:async()=>({ok:true,deviceId:'D000006'})};}
    throw new Error('unexpected request '+url);
  };
  runScript(scripts.Enroll,contextFor(storage,get,fetch,{search:'?session='+freshSession,pathname:'/enroll',hostname:'smartlock-04225a0ff0a4.local'}));
  await turn(); await turn();
  assert.equal(get('submitButton').hidden,false,'validated fresh grant permits recovery workflow');
  const oldPending=JSON.parse(storage.get('smartlock.enroll.pending.recovery.v1'))[0];
  assert.equal(oldPending.credential,stale.credential,'prior proof is retained in recovery storage');
  await get('submitButton').listeners.click();
  assert.notEqual(completeFields.credential,stale.credential,'new authorized invitation receives a fresh credential');
  assert.equal(completeFields.session,freshSession);
  assert.equal(JSON.parse(storage.get('smartlock.devices.v1'))[0].id,'D000006');
}

async function enrollmentTransientFailureCannotRotatePending() {
  const stale={state:'pending',session:'1'.repeat(64),credential:'2'.repeat(64),name:'old',role:'User'};
  const storage=new Map([['smartlock.enroll.pending.v1',JSON.stringify(stale)]]),get=elements(['submitButton']);
  runScript(scripts.Enroll,contextFor(storage,get,async url=>{
    if(url==='/api/registration/reconcile')throw new Error('transport lost');
    if(url.startsWith('/api/enroll/check'))return {ok:true,json:async()=>({ok:true,name:'new',role:'User',expiresIn:120})};
    throw new Error('unexpected request '+url);
  },{search:'?session='+('3'.repeat(64)),pathname:'/enroll',hostname:'smartlock-04225a0ff0a4.local'}));
  await turn(); await turn();
  assert.equal(get('submitButton').hidden,true,'fresh invitation cannot authorize credential rotation after transport failure');
  assert.equal(storage.get('smartlock.enroll.pending.v1'),JSON.stringify(stale));
  assert.equal(storage.has('smartlock.enroll.pending.recovery.v1'),false);
}

function managementOwnerCredentialPrecedence() {
  const begin=source.indexOf("  let saved=null;try{saved=JSON.parse",source.indexOf('static const char kManage'));
  const end=source.indexOf("  if(!credentials.length)",begin);
  const selection=source.slice(begin,end)+'\n  globalThis.selectedManagementCredentials=credentials;';
  const storage=new Map([
    ['smartlock.setup.v1',JSON.stringify({state:'active',credential:'a'.repeat(64)})],
    ['smartlock.devices.v1',JSON.stringify([{id:'D000002',credential:'b'.repeat(64)}])]
  ]);
  const context=contextFor(storage,elements(),async()=>({ok:false}),{search:'?session='+('c'.repeat(64)),pathname:'/manage',hostname:'smartlock-04225a0ff0a4.local'});
  vm.runInNewContext('(()=>{'+selection+';return globalThis.selectedManagementCredentials;})()',context);
  assert.equal(context.selectedManagementCredentials[0].id,'D000001','Management submits current Setup Owner first');
  assert.equal(context.selectedManagementCredentials[0].credential,'a'.repeat(64));
  assert.equal(context.selectedManagementCredentials[1].id,'D000002','stale device credential cannot override Owner');
}

(async()=>{
  await setupReconciliation();
  await setupCommitWithLostResponse();
  await setupFreshQrKeepsPendingCredential();
  await failedSetupProofPreservesStorage();
  await legacyPendingRecoversOnFirstVisit();
  await enrollmentLostResponseAndRevisit();
  await rejectedEnrollmentProofPreservesPending();
  await freshEnrollmentRotatesOnlyAfterDefinitiveProofFailure();
  await enrollmentTransientFailureCannotRotatePending();
  managementOwnerCredentialPrecedence();
  console.log('PASS: production Setup/Enrollment/Access/Management credential selection VM scenarios (commit response loss, same-ID recovery, stale Owner precedence, pending preservation, consumed/revoked/wrong proof)');
})().catch(error=>{console.error(error);process.exitCode=1;});
