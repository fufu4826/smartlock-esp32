// Execute actual browser deadline functions/scripts with deterministic suspended time.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync('src/web/WebAssets.h','utf8');
function script(name){return source.split('static const char k'+name+'[]')[1].match(/<script>([\s\S]*?)<\/script>/)[1];}
const manage=script('Manage'),enroll=script('Enroll');
let now=1000,callback,checks=0;
const nodes=new Map();function node(id){if(!nodes.has(id))nodes.set(id,{hidden:false,textContent:'',value:'',listeners:{},addEventListener(k,v){this.listeners[k]=v;},scrollIntoView(){}});return nodes.get(id);}
const ctx={URL,URLSearchParams,encodeURIComponent,Date:{now:()=>now},location:{href:'http://smartlock-04225a0ff0a4.local/manage'},document:{getElementById:node},clearInterval(){},setInterval(fn){callback=fn;return 1;},roleLabels:{User:'ผู้ใช้'},enrollExpiryTimer:0,enrollExpiryTick:null};
vm.createContext(ctx);
vm.runInContext(manage.match(/  function showEnrollment\([^\n]+/)[0],ctx);
const result={url:'http://smartlock-04225a0ff0a4.local/enroll?session=abc',qrSvg:'<svg/>',expiresIn:120,requestedAt:1000};
now=31000;ctx.showEnrollment(result,{name:'hi',role:'User'});
assert.match(node('enrollExpiry').textContent,/90/);checks++;
now=81000;callback();assert.match(node('enrollExpiry').textContent,/40/);checks++;
now=121000;ctx.enrollExpiryTick();assert.equal(node('enrollQr').hidden,true);checks++;
assert.equal(node('enrollUrl').hidden,true);checks++;
now=200000;ctx.showEnrollment(result,{name:'hi',role:'User'});assert.equal(node('enrollQr').hidden,true);checks++;
// Production page check includes response delay AND time spent checking saved credentials.
(async()=>{
  nodes.clear();now=1000;let focus,visible,completeCalls=0;
  const ectx={URLSearchParams,Uint8Array,Date:{now:()=>now},crypto:{getRandomValues(){}},
    window:{location:{search:'?session=abc'},crypto:{getRandomValues(){}},addEventListener(k,v){if(k==='focus')focus=v;}},
    document:{getElementById:node,addEventListener(k,v){if(k==='visibilitychange')visible=v;}},
    localStorage:{getItem(){return null;}},setInterval(fn){callback=fn;return 1;},clearInterval(){},
    fetch:async(url)=>{if(url.startsWith('/api/enroll/check')){now=31000;return{ok:true,json:async()=>({ok:true,name:'hi',role:'User',expiresIn:120})};}completeCalls++;throw Error('unexpected mutation');}};
  await vm.runInNewContext(enroll,ectx);await new Promise(setImmediate);await new Promise(setImmediate);
  assert.match(node('intro').textContent,/90/);checks++;
  now=61000;callback();assert.match(node('intro').textContent,/60/);checks++;
  now=121000;focus();assert.equal(node('submitButton').hidden,true);checks++;
  visible();await node('submitButton').listeners.click();assert.equal(completeCalls,0);checks++;
  const main=fs.readFileSync('src/main.cpp','utf8');
  assert.ok(main.includes('createSession(type, ttl, millis(), sessionToken)'));checks++;
  assert.ok(!main.includes('touchManager.update(now)'));checks++;
  assert.match(manage,/requestedAt=Date.now\(\)/);checks++;
  console.log('PASS: '+checks+' display checks: delayed response/callback, suspended resume, exact expiry, no expired submission; TFT fresh-time source ownership.');
})().catch(e=>{console.error(e);process.exitCode=1;});
