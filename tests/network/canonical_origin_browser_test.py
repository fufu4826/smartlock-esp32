"""Production assets; simulated transport change, real mobile Chromium origin/storage."""
import json,re,threading
from pathlib import Path
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from urllib.parse import parse_qs,urlsplit
from playwright.sync_api import sync_playwright

root=Path(__file__).resolve().parents[2]
source=(root/'src/web/WebAssets.h').read_text(encoding='utf-8')
assets={n:re.search(r'static const char k'+n+r'\[\] PROGMEM = R"HTML\((.*?)\)HTML";',source,re.S).group(1) for n in ['Manage','Access']}
credential='a'*64
requests=[]
class Handler(BaseHTTPRequestHandler):
 def log_message(self,*_):pass
 def send(self,status,value,mime='application/json'):
  b=(json.dumps(value) if mime=='application/json' else value).encode()
  self.send_response(status);self.send_header('Content-Type',mime);self.send_header('Content-Length',str(len(b)));self.end_headers();self.wfile.write(b)
 def do_GET(self):self.send(200,assets['Access' if self.path.startswith('/a/') else 'Manage'],'text/html; charset=utf-8')
 def do_POST(self):
  fields={k:v[-1] for k,v in parse_qs(self.rfile.read(int(self.headers['Content-Length'])).decode()).items()}
  requests.append((self.path,fields))
  if self.path in ['/api/manage/login','/api/access/request']:
   if fields.get('credential')!=credential or fields.get('deviceId')!='D000001':return self.send(403,{'error':'forbidden'})
   return self.send(200,{'ok':True,'token':'fixture-management','unlockSeconds':5})
  if fields.get('token')!='fixture-management':return self.send(403,{'error':'forbidden'})
  if self.path=='/api/manage/state':return self.send(200,{'actorRole':'Owner','lanEnrollment':False,'identities':[]})
  if self.path=='/api/manage/summary':return self.send(200,{'uptimeMs':1,'freeHeap':100000,'minFreeHeap':90000,'databaseHealthy':True,'firmware':'fixture','canonicalHost':'smartlock-04225a0ff0a4.local'})
  return self.send(404,{'error':'not_found'})

servers=[ThreadingHTTPServer(('127.0.0.1',0),Handler) for _ in range(2)]
for server in servers:threading.Thread(target=server.serve_forever,daemon=True).start()
try:
 with sync_playwright() as p:
  browser=p.chromium.launch(headless=True)
  context=browser.new_context(viewport={'width':390,'height':844},is_mobile=True,has_touch=True,user_agent='Mozilla/5.0 (Linux; Android 14) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Mobile Safari/537.36')
  target=[servers[0]]
  def route(r):
   response=context.request.fetch('http://127.0.0.1:'+str(target[0].server_port)+urlsplit(r.request.url).path,method=r.request.method,data=r.request.post_data,headers={'Content-Type':'application/x-www-form-urlencoded'})
   r.fulfill(response=response)
  context.route('http://smartlock-04225a0ff0a4.local/**',route)
  page=context.new_page();errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
  origin='http://smartlock-04225a0ff0a4.local'
  page.goto(origin+'/manage?session=fixture');page.evaluate('(c)=>localStorage.setItem("smartlock.setup.v1",JSON.stringify({state:"active",credential:c}))',credential)
  for server in servers:
   target[0]=server
   page.goto(origin+'/manage?session=fixture');page.wait_for_function('!document.getElementById("app").hidden')
   assert page.evaluate('location.origin')==origin
   assert page.evaluate('JSON.parse(localStorage.getItem("smartlock.setup.v1")).credential')==credential
   page.goto(origin+'/a/'+'b'*64);page.wait_for_function('document.getElementById("status").className==="ok"')
  assert len([r for r in requests if r[0]=='/api/access/request'])==2
  assert all(f.get('credential')==credential and f.get('deviceId')=='D000001' for path,f in requests if path in ['/api/manage/login','/api/access/request'])
  assert not errors,errors
  browser.close()
 evidence=root/'docs/phase_reports/evidence/local_fallback';evidence.mkdir(parents=True,exist_ok=True)
 (evidence/'mobile-origin.json').write_text(json.dumps({'result':'PASS','production_assets':['Manage','Access'],'transport':'two local fixture receivers representing LAN/AP; not DNS or physical Android proof','origin':origin,'unchanged_browser_credential':True,'no_credential_migration':True},indent=2))
 print('Production mobile Chromium Access/Management same-origin continuity PASS (synthetic network)')
finally:
 for server in servers:server.shutdown();server.server_close()
