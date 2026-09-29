"""Non-unlocking dashboard API rejection and page-shell checks."""
import sys,urllib.request,urllib.parse,urllib.error
origin='http://'+(sys.argv[1] if len(sys.argv)>1 else '192.168.1.179')
opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
fake='0'*64
for path,fields in [('/api/manage/login',{'session':fake,'deviceId':'D000001','credential':fake}),('/api/manage/state',{'token':fake}),('/api/manage/summary',{'token':fake}),('/api/manage/users',{'token':fake,'name':'Negative','role':'Owner'}),('/api/manage/enroll',{'token':fake,'userId':'U000001','deviceName':'Negative'}),('/api/manage/revoke',{'token':fake,'deviceId':'D000001'}),('/api/network/status',{'token':fake}),('/api/network/connect',{'token':fake,'ssid':'Negative','password':'invalid123'})]:
 req=urllib.request.Request(origin+path,urllib.parse.urlencode(fields).encode(),{'Content-Type':'application/x-www-form-urlencoded'})
 try:
  opener.open(req,timeout=5)
  raise AssertionError(path+' accepted unauthorized request')
 except urllib.error.HTTPError as e:
  assert e.code==403,(path,e.code)
 print(path+': unauthorized DENIED')
page=opener.open(origin+'/manage',timeout=5).read().decode('utf-8')
assert 'lang="th"' in page
for panel in ['dashboardPanel','managementPanel','networkPanel','lockSettingsPanel','displayPanel','logsPanel','cloudPanel','systemPanel']:
 assert 'id="'+panel+'"' in page,panel
assert '/api/manage/summary' in page
print('Eight Thai dashboard panels: PASS (authenticated browser interaction still required)')
