"""Focused real AdminPin/PhysicalAdmin tests with isolated NVS and crypto fakes."""
from pathlib import Path
import tempfile,subprocess,sys,os
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/identity'))
from run_identity_tests import find_msvc_environment
with tempfile.TemporaryDirectory(prefix='admin-tests-') as tmp:
 t=Path(tmp);(t/'mbedtls').mkdir()
 (t/'Arduino.h').write_text('''#pragma once
#include <cstdint>
#include <cstddef>
#define HIGH 1
#define LOW 0
#define OUTPUT 1
extern uint32_t clockMs;extern uint32_t verificationDelay;extern int lockLevel;inline uint32_t millis(){return clockMs;}
inline void digitalWrite(int pin,int value){if(pin==22)lockLevel=value;}inline void pinMode(int,int){}
struct Log{void println(const char*){}};static Log Serial;
''')
 (t/'esp_timer.h').write_text('''#pragma once
#define ESP_OK 0
#define ESP_TIMER_TASK 0
typedef void* esp_timer_handle_t;
struct esp_timer_create_args_t{void(*callback)(void*);void*arg;int dispatch_method;const char*name;};
inline int esp_timer_create(const esp_timer_create_args_t*,esp_timer_handle_t*out){*out=(void*)1;return 0;}
inline int esp_timer_stop(esp_timer_handle_t){return 0;}inline int esp_timer_start_once(esp_timer_handle_t,uint64_t){return 0;}
''')
 (t/'Preferences.h').write_text('''#pragma once
#include <map>
#include <string>
#include <cstring>
extern std::map<std::string,std::string> records;extern bool failed;
class Preferences{public:bool begin(const char*,bool){return !failed;}void end(){}
bool isKey(const char*k){return records.find(k)!=records.end()&&!records[k].empty();}
size_t getBytesLength(const char*k){return records[k].size();}
size_t getBytes(const char*k,void*p,size_t n){if(records[k].size()!=n)return 0;memcpy(p,records[k].data(),n);return n;}
size_t putBytes(const char*k,const void*p,size_t n){if(failed)return 0;records[k]=std::string((const char*)p,n);return n;}
uint8_t getUChar(const char*k,uint8_t d){return records[k].empty()?d:(uint8_t)records[k][0];}
size_t putUChar(const char*k,uint8_t v){if(failed)return 0;records[k]=std::string(1,(char)v);return 1;}};
''')
 (t/'esp_system.h').write_text('#pragma once\n#include <cstring>\ninline void esp_fill_random(void*p,size_t n){memset(p,42,n);}\n')
 (t/'mbedtls/md.h').write_text('''#pragma once
#define MBEDTLS_MD_SHA256 6
struct mbedtls_md_context_t{};inline void mbedtls_md_init(mbedtls_md_context_t*){}inline const void* mbedtls_md_info_from_type(int){return nullptr;}inline int mbedtls_md_setup(mbedtls_md_context_t*,const void*,int){return 0;}inline void mbedtls_md_free(mbedtls_md_context_t*){}
''')
 (t/'mbedtls/pkcs5.h').write_text('''#pragma once
#include "md.h"
inline int mbedtls_pkcs5_pbkdf2_hmac(mbedtls_md_context_t*,const unsigned char*p,size_t,const unsigned char*s,size_t,unsigned int,size_t n,unsigned char*out){clockMs+=verificationDelay;for(size_t i=0;i<n;++i)out[i]=p[i%4]^s[i%16];return 0;}
''')
 test=ROOT/'tests/admin/admin_test.cpp';exe=t/'test.exe';vc=find_msvc_environment();assert vc
 (t/'TFT_eSPI.h').write_text('''#pragma once
#include <cstdint>
class TFT_eSPI{public:bool down=false;uint16_t x=0,y=0;bool getTouch(uint16_t*a,uint16_t*b,uint16_t){*a=x;*b=y;return down;}};
''')
 sources=[test,ROOT/'tests/admin/admin_transition_test.cpp',ROOT/'src/security/AdminPin.cpp',ROOT/'src/app/PhysicalAdmin.cpp',ROOT/'src/app/AppStateMachine.cpp',ROOT/'src/hardware/TouchManager.cpp',ROOT/'src/hardware/LockController.cpp',ROOT/'src/storage/ConfigStore.cpp']
 cmd=t/'build.cmd';cmd.write_text(f'call "{vc}" -arch=x64 -host_arch=x64\ncl /nologo /std:c++20 /EHsc /utf-8 /I"{t}" '+ ' '.join('"'+str(p)+'"' for p in sources)+f' /Fe:"{exe}"\n',encoding='utf-8')
 env=os.environ.copy();env['PATH']=r'C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem'
 subprocess.run(['cmd','/d','/c',str(cmd)],cwd=t,check=True,env=env)
 subprocess.run([str(exe)],check=True)
