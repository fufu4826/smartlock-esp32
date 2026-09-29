const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const text=fs.readFileSync('src/web/WebAssets.h','utf8');
const part=text.slice(text.indexOf('  async function refreshLogsStatus(){'),text.indexOf('  let networkPollTimer='));
const elements=new Map();const get=id=>{if(!elements.has(id))elements.set(id,{value:'',children:[],replaceChildren(){this.children=[];},appendChild(item){this.children.push(item);if(!this.value)this.value=item.value;}});return elements.get(id);};
let exported,download,revoked=false;
const segments=[{id:'old.csv',records:32},{id:'middle.csv',records:32},{id:'new.csv',records:2}];
const context={token:'fixture',logsResult:{},renderLogsStatus(){},formBody:x=>new URLSearchParams(x).toString(),Blob,
  document:{getElementById:get,createElement(tag){return tag==='a'?{click(){download=this.download;},remove(){}}:{};},body:{appendChild(){}}},
  URL:{createObjectURL:()=> 'blob:fixture',revokeObjectURL(){revoked=true;}},setTimeout(fn){fn();},
  request:async path=>path.endsWith('/segments')?{segments,oldest:'old.csv',newest:'new.csv',historyLostEvents:64,historyLostSegments:2}:{},
  fetch:async(path,options)=>{exported=new URLSearchParams(options.body);return{ok:true,status:200,headers:{get:()=> 'text/csv'},text:async()=> 'event_id\nfixture\n'};}};
vm.createContext(context);vm.runInContext(part,context);
(async()=>{await context.refreshLogsStatus();assert.equal(get('logSegment').children.length,3);assert.match(get('logHistory').textContent,/old.csv/);assert.match(get('logHistory').textContent,/new.csv/);assert.match(get('logHistory').textContent,/64/);
  for(const segment of segments){get('logSegment').value=segment.id;await context.exportOldestLog();assert.equal(exported.get('segment'),segment.id);assert.equal(exported.get('token'),'fixture');assert.equal(download,segment.id);}
  assert.equal(revoked,true);console.log('PASS: 14 production browser log checks: enumeration, history-loss visibility, oldest/middle/newest named export, bounded download cleanup.');
})().catch(e=>{console.error(e);process.exitCode=1;});
