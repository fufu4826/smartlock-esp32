"""Compile production Setup/AuthStore method bodies against isolated host dependencies."""
from pathlib import Path
import tempfile,re,subprocess,sys,os
sys.path.insert(0,str(Path('tests/identity').resolve()))
from run_identity_tests import find_msvc_environment
root=Path.cwd()
def strip(s):return re.sub(r'^\s*#(?:include[^\n]*|pragma once)\s*$', '',s,flags=re.M)
with tempfile.TemporaryDirectory(prefix='setup-focused-') as td:
 t=Path(td)
 code=r'''
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <cassert>
#include <cstdio>
uint32_t millis(){return 100;}
std::map<std::string,std::vector<uint8_t>> files;
struct AtomicFileStore{static constexpr size_t kMaxPayload=8192;enum class ReadResult{CurrentValid,Missing,Corrupt};static ReadResult read(const char*p,uint8_t*out,size_t cap,size_t&n){if(!files.count(p)){n=0;return ReadResult::Missing;}n=files[p].size();if(n>cap)return ReadResult::Corrupt;memcpy(out,files[p].data(),n);return ReadResult::CurrentValid;}static bool write(const char*p,const uint8_t*b,size_t n){files[p]={b,b+n};return true;}};
struct RequestPolicy{static bool deviceId(const char*p){return p&&strlen(p)==7&&p[0]=='D';}};
using wifi_mode_t=int;constexpr int WIFI_MODE_NULL=0,ESP_OK=0;int esp_wifi_get_mode(int*p){*p=1;return 0;}
void esp_fill_random(void*p,size_t n){static uint8_t v=1;memset(p,v++,n);}
// Deterministic stand-in: validates storage/control flow, not cryptographic strength.
int mbedtls_sha256_ret(const uint8_t*p,size_t n,uint8_t*out,int){memset(out,0,32);for(size_t i=0;i<n;i++)out[i%32]^=p[i];return 0;}
'''
 code+=strip((root/'src/storage/AuthStore.h').read_text())+strip((root/'src/storage/AuthStore.cpp').read_text())
 code+=r'''
enum class SessionType{Setup};
struct SessionManager{static constexpr size_t kTokenChars=32;bool valid=true;bool validateSession(SessionType,const char*,uint32_t){return valid;}bool consumeSession(SessionType,const char*,uint32_t){bool b=valid;valid=false;return b;}};
struct SmartLockConfig{bool configured=false,ownerExists=false;uint32_t unlockDurationMs=0;int networkMode=0;};
struct ConfigStore{SmartLockConfig c;bool healthy(){return true;}const SmartLockConfig&config(){return c;}bool save(const SmartLockConfig&v){c=v;return true;}};
struct StorageHealth{bool available(){return true;}};
struct NetworkSecrets{enum class Result{Valid,Missing};std::string pw;bool saveSetupPassword(const char*p){pw=p;return true;}Result load(char(&p)[33]){strcpy(p,pw.c_str());return pw.empty()?Result::Missing:Result::Valid;}};
struct AdminPin{static inline std::string pin;static bool valid(const char*p){if(strlen(p)!=4)return false;for(int i=0;i<4;i++)if(p[i]<'0'||p[i]>'9')return false;return true;}static bool initialize(const char*p){if(!pin.empty())return false;pin=p;return true;}};
namespace smartlock{namespace storage{struct IdentityStore{static inline std::string name;static bool modePresent(){return !name.empty();}static bool normalizeName(const char*p,char(&out)[41]){if(!p||!strlen(p)||strlen(p)>40)return false;strcpy(out,p);return true;}static bool healthy(){return !name.empty()&&AuthStore::hasDevice("D000001");}static bool createFirst(const char*p){if(!name.empty())return false;name=p;return true;}};}}
'''
 code+=strip((root/'src/app/FirstOwnerSetup.h').read_text())+strip((root/'src/app/FirstOwnerSetup.cpp').read_text())
 code+=r'''
int main(){using R=FirstOwnerSetup::Result;ConfigStore c;StorageHealth h;SessionManager sess;NetworkSecrets net;FirstOwnerSetup setup(c,h,sess,net);FirstOwnerInput in={};strcpy(in.ownerName,"Only Owner");memset(in.credential,'a',64);memset(in.apPassword,'b',32);in.unlockSeconds=5;
for(const char*p:{"","123","12x4"}){strcpy(in.adminPin,p);strcpy(in.pinConfirm,p);assert(setup.complete(in,0)==R::InvalidInput);assert(files.empty()&&sess.valid&&AdminPin::pin.empty());}
strcpy(in.adminPin,"5678");strcpy(in.pinConfirm,"5679");assert(setup.complete(in,0)==R::InvalidInput);strcpy(in.pinConfirm,"5678");assert(setup.complete(in,0)==R::Success);assert(c.c.configured&&c.c.ownerExists&&c.c.unlockDurationMs==5000);assert(IdentityStore::name=="Only Owner");assert(AdminPin::pin=="5678");size_t n=0;assert(AuthStore::inspect(n)==AuthStore::ReadResult::Valid&&n==1);assert(AuthStore::verifyFirst("D000001",in.credential));assert(!AuthStore::saveFirst("D000001",in.credential));assert(setup.complete(in,0)==R::AlreadyConfigured);
// Header contains only magic/schema/count; no legacy passphrase salt/hash.
auto snapshot=files["/smartlock/db/auth.rec"];assert(snapshot.size()==12+57);assert(snapshot[4]==2);assert(!AuthStore::addDevice("D000002",in.credential));char other[65]={};memset(other,'c',64);assert(AuthStore::addDevice("D000002",other));assert(AuthStore::verifyDevice("D000002",other));assert(!AuthStore::verifyDevice("D000001",other));assert(AuthStore::inspect(n)==AuthStore::ReadResult::Valid&&n==2);
files["/smartlock/backups/test"]=snapshot;assert(AuthStore::ownerVerifierMatches("/smartlock/backups/test"));files["/smartlock/db/auth.rec"][4]=1;assert(AuthStore::inspect(n)==AuthStore::ReadResult::Invalid);assert(!AuthStore::verifyDevice("D000001",in.credential));puts("Production Setup/AuthStore focused tests PASS (isolated dependencies; no live Owner creation)");}
'''
 (t/'test.cpp').write_text(code,encoding='utf-8')
 cmd=t/'build.cmd';cmd.write_text(f'call "{find_msvc_environment()}" -arch=x64 -host_arch=x64\ncl /nologo /std:c++17 /EHsc /utf-8 test.cpp /Fe:test.exe\n',encoding='utf-8')
 env=os.environ.copy();env['PATH']=r'C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem'
 subprocess.run(['cmd','/d','/c',str(cmd)],cwd=t,env=env,check=True)
 subprocess.run([str(t/'test.exe')],check=True)
