#include "generation_store_test_support.h"
#include "Preferences.h"
#include "esp_system.h"
#include "RecordCodec.h"
#include "MaintenanceBarrier.h"
#include <map>
#include <string>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <new>

SDClass SD;
FakeEsp ESP;
static uint32_t fakeNow=100;
uint32_t millis(){return fakeNow;}
static uint32_t randomValue=100;
static char captureSsid[33]="factory-net";
static char captureAp[33]="0123456789abcdef0123456789abcdef";
uint32_t esp_random(){randomValue+=17;return randomValue;}

namespace {
std::map<std::string,std::string> nvs;
size_t putOperations=0,failOperation=(size_t)-1;
bool failAfter=false;
std::string nvsKey(const std::string& ns,const char* key){return ns+"/"+(key?key:"");}
bool copyRecord(const SecureBackup::Buffer& b,SecureBackup::Snapshot& s,const uint8_t*& tail){
  if(!b.bytes||b.size<sizeof(s))return false;
  memcpy(&s,b.bytes.get(),sizeof(s));
  if(b.size!=sizeof(s)+s.identitiesLength+s.authLength)return false;
  tail=b.bytes.get()+sizeof(s);return true;
}
bool snapshotValid(const uint8_t* bytes,size_t length,uint64_t board,size_t& identities){
  identities=0;if(!bytes||length<sizeof(SecureBackup::Snapshot))return false;
  SecureBackup::Snapshot s={};memcpy(&s,bytes,sizeof(s));
  if(memcmp(s.magic,"SLSNAP01",8)||s.version!=1||s.board!=board||
     length!=sizeof(s)+s.identitiesLength+s.authLength||
     s.identitiesLength==0||s.authLength==0||!ConfigStore::validateSnapshot(s.config))return false;
  uint32_t expected=smartlock::storage::recordCrc32(bytes+offsetof(SecureBackup::Snapshot,board),length - offsetof(SecureBackup::Snapshot,board));
  if(expected!=s.crc||!s.config.configured||!s.config.ownerExists||s.staPresent>1||s.calPresent>1||!memchr(s.apPassword,0,sizeof(s.apPassword))||
     !memchr(s.ssid,0,sizeof(s.ssid))||!memchr(s.staPassword,0,sizeof(s.staPassword)))return false;
  if(strlen(s.apPassword)!=32)return false;
  for(size_t i=0;i<32;++i)if(!((s.apPassword[i]>='0'&&s.apPassword[i]<='9')||(s.apPassword[i]>='a'&&s.apPassword[i]<='f')))return false;
  if(s.staPresent&&(strlen(s.ssid)==0||strlen(s.ssid)>32||strlen(s.staPassword)<8||strlen(s.staPassword)>63))return false;
  identities=s.identitiesLength;return true;
}
}
// Fake NVS is byte durable across GenerationStore::begin() calls. This models
// putBytes commits and torn/failed writes, not ESP Preferences internals.
namespace FakePreferences {
void clear(){nvs.clear();putOperations=0;failOperation=(size_t)-1;failAfter=false;}
void failPutAt(size_t operation,bool afterCommit){putOperations=0;failOperation=operation;failAfter=afterCommit;}
void disableFault(){failOperation=(size_t)-1;failAfter=false;}
size_t putCount(){return putOperations;}
bool corrupt(const char* name,size_t offset){auto it=nvs.find(nvsKey("sl-generation",name));if(it==nvs.end()||offset>=it->second.size())return false;it->second[offset]^=1;return true;}
}
bool Preferences::begin(const char* name,bool ro){if(!name)return false;name_=name;open_=true;readOnly_=ro;return true;}
bool Preferences::isKey(const char* key)const{return open_&&key&&nvs.count(nvsKey(name_,key));}
size_t Preferences::getBytesLength(const char* key)const{if(!open_||!key)return 0;auto it=nvs.find(nvsKey(name_,key));return it==nvs.end()?0:it->second.size();}
size_t Preferences::getBytes(const char* key,void* output,size_t length)const{if(!open_||!key||!output)return 0;auto it=nvs.find(nvsKey(name_,key));if(it==nvs.end()||length<it->second.size())return 0;memcpy(output,it->second.data(),it->second.size());return it->second.size();}
size_t Preferences::putBytes(const char* key,const void* input,size_t length){if(!open_||readOnly_||!key||!input)return 0;size_t op=putOperations++;if(op==failOperation&&!failAfter)return 0;nvs[nvsKey(name_,key)]=std::string((const char*)input,length);if(op==failOperation&&failAfter)return 0;return length;}
bool Preferences::clear(){if(!open_||readOnly_)return false;for(auto it=nvs.begin();it!=nvs.end();)if(it->first.compare(0,name_.size()+1,name_+"/")==0)it=nvs.erase(it);else ++it;return true;}
void Preferences::end(){open_=false;}

SecureBackup::Buffer::~Buffer(){if(bytes&&size){volatile uint8_t* p=bytes.get();for(size_t i=0;i<size;++i)p[i]=0;}}
bool SecureBackup::Buffer::allocate(size_t n){bytes.reset(n?new(std::nothrow) uint8_t[n]():nullptr);size=n;return n==0||bytes!=nullptr;}

void resetGenerationFakes(){SD.clear();FakePreferences::clear();MaintenanceBarrier::release();randomValue=100;fakeNow=100;strcpy(captureSsid,"factory-net");strcpy(captureAp,"0123456789abcdef0123456789abcdef");}
void setCaptureNetwork(const char* ssid,const char* ap){snprintf(captureSsid,sizeof(captureSsid),"%s",ssid?ssid:"");snprintf(captureAp,sizeof(captureAp),"%s",ap?ap:"");}
static const char identities[]="U1|D000001|Owner|OWNER|ACTIVE|00000000\n";
static const char auth[]="D000001|opaque-verifier-owner\n";
void makeCurrentSnapshot(const SmartLockConfig& config,SecureBackup::Buffer& out,const char* ssid,const char* ap){
  out.size=sizeof(SecureBackup::Snapshot)+sizeof(identities)-1+sizeof(auth)-1;out.bytes.reset(new uint8_t[out.size]());
  SecureBackup::Snapshot s={};memcpy(s.magic,"SLSNAP01",8);s.version=1;s.board=ESP.getEfuseMac();s.identitiesLength=sizeof(identities)-1;s.authLength=sizeof(auth)-1;s.config=config;
  snprintf(s.apPassword,sizeof(s.apPassword),"%s",ap);snprintf(s.ssid,sizeof(s.ssid),"%s",ssid);strcpy(s.staPassword,"wifi-password-123");s.staPresent=1;s.calPresent=1;for(size_t i=0;i<5;++i)s.calibration[i]=(uint16_t)(100+i);
  memcpy(out.bytes.get(),&s,sizeof(s));memcpy(out.bytes.get()+sizeof(s),identities,sizeof(identities)-1);memcpy(out.bytes.get()+sizeof(s)+sizeof(identities)-1,auth,sizeof(auth)-1);
  s.crc=smartlock::storage::recordCrc32(out.bytes.get()+offsetof(SecureBackup::Snapshot,board),out.size - offsetof(SecureBackup::Snapshot,board));memcpy(out.bytes.get(),&s,sizeof(s));
}
bool SecureBackup::validatePlain(const uint8_t* p,size_t n,uint64_t board,size_t& identitiesLength){return snapshotValid(p,n,board,identitiesLength);}
bool SecureBackup::capture(const ConfigStore& config,Buffer& out){makeCurrentSnapshot(config.config(),out,captureSsid,captureAp);return true;}
bool SecureBackup::sameOwner(const Buffer& a,const Buffer& b){SecureBackup::Snapshot x={},y={};const uint8_t *xt=nullptr,*yt=nullptr;if(!copyRecord(a,x,xt)||!copyRecord(b,y,yt)||x.identitiesLength!=y.identitiesLength||x.authLength!=y.authLength)return false;return !memcmp(xt,yt,x.identitiesLength+x.authLength);}

void seedLegacyFiles(){
  const char identity[]="IDENTITY_V1_COMMITTED";
  AtomicFileStore::write("/smartlock/db/identity-mode.rec",(const uint8_t*)identity,sizeof(identity)-1);
  AtomicFileStore::write("/smartlock/db/identities.rec",(const uint8_t*)identities,sizeof(identities)-1);
  AtomicFileStore::write("/smartlock/db/auth.rec",(const uint8_t*)auth,sizeof(auth)-1);
}
bool verifySelectedGeneration(const SmartLockConfig& expected,const char* ssid,const char* ap){
  if(!GenerationStore::active())return false;SmartLockConfig got={};if(!GenerationStore::loadConfig(got)||memcmp(&got,&expected,sizeof(got)))return false;
  char apGot[33]={},ssidGot[33]={},pass[65]={};bool sta=false;if(!GenerationStore::loadNetwork(apGot,ssidGot,pass,sta))return false;
  if(strcmp(apGot,ap)||strcmp(ssidGot,ssid)||!sta)return false;
  uint8_t data[128];size_t len=0;return AtomicFileStore::read(GenerationStore::authPath(),data,sizeof(data),len)==AtomicFileStore::ReadResult::CurrentValid&&len==sizeof(auth)-1&&!memcmp(data,auth,len);
}
bool verifyLegacyGeneration(const SmartLockConfig& expected,const char*,const char*){
  if(GenerationStore::active())return false;uint8_t data[128];size_t len=0;return AtomicFileStore::read(GenerationStore::authPath(),data,sizeof(data),len)==AtomicFileStore::ReadResult::CurrentValid&&len==sizeof(auth)-1&&!memcmp(data,auth,len)&&ConfigStore::validateSnapshot(expected);
}
