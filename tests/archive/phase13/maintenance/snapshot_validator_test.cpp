#include "SecureBackup.h"
#include "AuthStore.h"
#include "IdentityStore.h"
#include "RecordCodec.h"
#include "ConfigStore.h"
#include "GenerationStore.h"
#include <Arduino.h>
#include <SD.h>
#include <cstddef>
#include <new>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

SerialClass Serial;
SDClass SD;
uint32_t millis() { return 0; }
using namespace smartlock::storage;

namespace {
int assertions=0, failures=0;
void check(bool value,const char* label) {
  ++assertions;
  if(!value){++failures;std::fprintf(stderr,"FAIL: %s\n",label);}
}
uint32_t crc32(const uint8_t* bytes,size_t length) {
  uint32_t crc=0xffffffffu;
  for(size_t i=0;i<length;++i){crc^=bytes[i];for(int b=0;b<8;++b)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));}
  return ~crc;
}
struct AuthHeader {uint32_t magic,schema,iterations,count;uint8_t adminSalt[16],adminHash[32];};
struct AuthEntry {char id[9];uint8_t salt[16],hash[32];};
static_assert(sizeof(AuthHeader)==64,"fixture auth header ABI");
static_assert(sizeof(AuthEntry)==57,"fixture auth entry ABI");
struct Fixture {
  SecureBackup::Buffer buffer;
  std::vector<IdentityRecord> rows;
  std::vector<AuthEntry> auth;
};
void addIdentity(Fixture& f,const char* id,const char* name,UserRole role=UserRole::Owner,
                 RecordStatus status=RecordStatus::Active,unsigned key=1) {
  IdentityRecord r={};std::snprintf(r.id,sizeof(r.id),"%s",id);std::snprintf(r.name,sizeof(r.name),"%s",name);r.role=role;r.status=status;f.rows.push_back(r);
  AuthEntry a={};std::snprintf(a.id,sizeof(a.id),"%s",id);
  for(size_t i=0;i<sizeof(a.salt);++i)a.salt[i]=static_cast<uint8_t>(key+i+1);
  for(size_t i=0;i<sizeof(a.hash);++i)a.hash[i]=static_cast<uint8_t>(key+i+41);
  f.auth.push_back(a);
}
std::string encodeLine(const IdentityRecord& row) {
  char line[kMaxRecordLineLength+1]={};size_t n=0;
  if(!encodeIdentityRecord(row,line,sizeof(line),&n))return {};
  return std::string(line,n);
}
std::string roleName(UserRole role) {
  switch(role){case UserRole::Owner:return "OWNER";case UserRole::Admin:return "ADMIN";case UserRole::User:return "USER";case UserRole::Guest:return "GUEST";}
  return "INVALID";
}
std::string statusName(RecordStatus status){return status==RecordStatus::Active?"ACTIVE":"REVOKED";}
std::string rawLine(const char* id,const char* name,const char* role,const char* status) {
  std::string prefix=std::string("I1|")+id+"|"+name+"|"+role+"|"+status+"|";
  char crc[9];std::snprintf(crc,sizeof(crc),"%08X",recordCrc32(reinterpret_cast<const uint8_t*>(prefix.data()),prefix.size()));
  return prefix+crc;
}
bool build(Fixture& f,const std::vector<std::string>* rawRows=nullptr) {
  std::string identities;
  if(rawRows){for(const auto& line:*rawRows){identities+=line;identities+='\n';}}
  else for(const auto& row:f.rows){identities+=encodeLine(row);identities+='\n';}
  AuthHeader h={};h.magic=0x48545541u;h.schema=1;h.iterations=AuthStore::kAdminIterations;h.count=static_cast<uint32_t>(f.auth.size());
  for(size_t i=0;i<sizeof(h.adminSalt);++i)h.adminSalt[i]=static_cast<uint8_t>(i+1);
  for(size_t i=0;i<sizeof(h.adminHash);++i)h.adminHash[i]=static_cast<uint8_t>(i+31);
  std::vector<uint8_t> auth(sizeof(h)+f.auth.size()*sizeof(AuthEntry));
  std::memcpy(auth.data(),&h,sizeof(h));
  for(size_t i=0;i<f.auth.size();++i)std::memcpy(auth.data()+sizeof(h)+i*sizeof(AuthEntry),&f.auth[i],sizeof(AuthEntry));
  SecureBackup::Snapshot snap={};std::memcpy(snap.magic,"SLSNAP01",8);snap.version=1;snap.board=0x1122334455667788ull;
  snap.identitiesLength=static_cast<uint32_t>(identities.size());snap.authLength=static_cast<uint32_t>(auth.size());
  snap.config.schemaVersion=1;snap.config.generation=1;snap.config.unlockDurationMs=10000;snap.config.accessQrLifetimeMs=30000;
  snap.config.managementQrLifetimeMs=90000;snap.config.screenTimeoutMs=30000;snap.config.configured=1;snap.config.ownerExists=1;
  std::memcpy(snap.config.deviceName,"SmartLock",10);
  snap.config.crc32=crc32(reinterpret_cast<const uint8_t*>(&snap.config),offsetof(SmartLockConfig,crc32));
  std::memcpy(snap.apPassword,"0123456789abcdef0123456789abcdef",33);
  const size_t total=sizeof(snap)+identities.size()+auth.size();if(!f.buffer.allocate(total))return false;
  std::memcpy(f.buffer.bytes.get(),&snap,sizeof(snap));size_t p=sizeof(snap);
  std::memcpy(f.buffer.bytes.get()+p,identities.data(),identities.size());p+=identities.size();
  std::memcpy(f.buffer.bytes.get()+p,auth.data(),auth.size());f.buffer.size=total;
  auto* stored=reinterpret_cast<SecureBackup::Snapshot*>(f.buffer.bytes.get());
  stored->crc=recordCrc32(f.buffer.bytes.get()+offsetof(SecureBackup::Snapshot,board),total-offsetof(SecureBackup::Snapshot,board));
  return true;
}
bool valid(Fixture& f,uint64_t board=0x1122334455667788ull){size_t count=999;const bool result=SecureBackup::validatePlain(f.buffer.bytes.get(),f.buffer.size,board,count);return result;}
void owner(Fixture& f){addIdentity(f,"D000001","gugy");build(f);}
void withGuest(Fixture& f){addIdentity(f,"D000001","gugy");addIdentity(f,"D000002","Boom",UserRole::Guest,RecordStatus::Active,20);build(f);}
}

// Definitions required by the production Buffer and the unexercised store paths.
SecureBackup::Buffer::~Buffer()=default;
bool SecureBackup::Buffer::allocate(size_t n){std::unique_ptr<uint8_t[]> next(new(std::nothrow) uint8_t[n]{});if(!next)return false;bytes=std::move(next);size=n;return true;}
bool GenerationStore::initialized(){return false;}
bool GenerationStore::ready(){return true;}
bool GenerationStore::active(){return false;}
const char* GenerationStore::modePath(){return "/smartlock/db/identity-mode.rec";}
const char* GenerationStore::identityPath(){return "/smartlock/db/identities.rec";}
bool GenerationStore::loadConfig(SmartLockConfig&){return false;}
bool GenerationStore::saveConfig(const SmartLockConfig&){return false;}
bool GenerationStore::loadNetwork(char (&)[33],char (&)[33],char (&)[65],bool&){return false;}
bool GenerationStore::saveSta(const char*,const char*){return false;}
AuthStore::ReadResult AuthStore::inspect(size_t& count){count=0;return ReadResult::Missing;}
bool AuthStore::hasDevice(const char*){return false;}
bool AuthStore::ownerVerifierMatches(const char*){return false;}
namespace {
std::map<std::string,std::string> nvsBytes;
std::string nvsName(const std::string& name,const char* key){return name+"/"+(key?key:"");}
}
namespace FakePreferences {void clear(){nvsBytes.clear();}void failPutAt(size_t,bool){}void disableFault(){}size_t putCount(){return 0;}bool corrupt(const char*,size_t){return false;}}
bool Preferences::begin(const char* name,bool){name_=name?name:"";open_=true;return true;}
bool Preferences::isKey(const char* key)const{return open_&&nvsBytes.count(nvsName(name_,key))!=0;}
size_t Preferences::getBytesLength(const char* key)const{auto i=nvsBytes.find(nvsName(name_,key));return i==nvsBytes.end()?0:i->second.size();}
size_t Preferences::getBytes(const char* key,void* out,size_t cap)const{auto i=nvsBytes.find(nvsName(name_,key));if(!open_||i==nvsBytes.end()||(!out&&cap))return 0;size_t n=i->second.size()<cap?i->second.size():cap;if(n)std::memcpy(out,i->second.data(),n);return n;}
size_t Preferences::putBytes(const char* key,const void* data,size_t n){if(!open_||readOnly_||(!data&&n))return 0;nvsBytes[nvsName(name_,key)]=std::string(static_cast<const char*>(data),n);return n;}
bool Preferences::clear(){if(!open_||readOnly_)return false;for(auto i=nvsBytes.begin();i!=nvsBytes.end();)if(i->first.compare(0,name_.size()+1,name_+"/")==0)i=nvsBytes.erase(i);else ++i;return true;}
void Preferences::end(){open_=false;}

int main(){
  Fixture baseline;owner(baseline);check(valid(baseline),"valid one-Owner snapshot");
  Fixture missing;addIdentity(missing,"D000002","Guest",UserRole::Guest);build(missing);check(!valid(missing),"missing Owner");
  Fixture multiple;addIdentity(multiple,"D000001","gugy");addIdentity(multiple,"D000002","second",UserRole::Owner);build(multiple);check(!valid(multiple),"multiple Owners");
  Fixture revoked;addIdentity(revoked,"D000001","gugy",UserRole::Owner,RecordStatus::Revoked);build(revoked);check(!valid(revoked),"revoked Owner");
  Fixture dupName;addIdentity(dupName,"D000001","gugy");addIdentity(dupName,"D000002","gugy",UserRole::Guest,RecordStatus::Active,20);build(dupName);check(!valid(dupName),"duplicate identity name");
  Fixture dupId;addIdentity(dupId,"D000001","gugy");addIdentity(dupId,"D000001","other",UserRole::Guest,RecordStatus::Active,20);build(dupId);check(!valid(dupId),"duplicate identity ID");
  Fixture missingAuth;addIdentity(missingAuth,"D000001","gugy");missingAuth.auth.clear();build(missingAuth);check(!valid(missingAuth),"missing auth verifier");
  Fixture orphan;addIdentity(orphan,"D000001","gugy");AuthEntry extra={};std::strcpy(extra.id,"D000002");extra.salt[0]=1;extra.hash[0]=2;orphan.auth.push_back(extra);build(orphan);check(!valid(orphan),"orphan auth verifier");
  Fixture badRole;addIdentity(badRole,"D000001","gugy");auto roleRows=std::vector<std::string>{rawLine("D000001","gugy","BROKEN","ACTIVE")};build(badRole,&roleRows);check(!valid(badRole),"malformed role");
  Fixture badStatus;addIdentity(badStatus,"D000001","gugy");auto statusRows=std::vector<std::string>{rawLine("D000001","gugy","OWNER","BROKEN")};build(badStatus,&statusRows);check(!valid(badStatus),"malformed status");
  Fixture badConfig;owner(badConfig);reinterpret_cast<SecureBackup::Snapshot*>(badConfig.buffer.bytes.get())->config.generation=0;
  auto* bc=reinterpret_cast<SecureBackup::Snapshot*>(badConfig.buffer.bytes.get());bc->config.crc32=crc32(reinterpret_cast<const uint8_t*>(&bc->config),offsetof(SmartLockConfig,crc32));
  bc->crc=recordCrc32(badConfig.buffer.bytes.get()+offsetof(SecureBackup::Snapshot,board),badConfig.buffer.size-offsetof(SecureBackup::Snapshot,board));check(!valid(badConfig),"malformed config structure");
  Fixture badConfigCrc;owner(badConfigCrc);auto* bcc=reinterpret_cast<SecureBackup::Snapshot*>(badConfigCrc.buffer.bytes.get());bcc->config.crc32^=1;
  bcc->crc=recordCrc32(badConfigCrc.buffer.bytes.get()+offsetof(SecureBackup::Snapshot,board),badConfigCrc.buffer.size- offsetof(SecureBackup::Snapshot,board));check(!valid(badConfigCrc),"malformed config CRC");
  check(!valid(baseline,0x8877665544332211ull),"wrong board ID");
  Fixture badCrc;owner(badCrc);badCrc.buffer.bytes[offsetof(SecureBackup::Snapshot,crc)]^=0x01;check(!valid(badCrc),"corrupted snapshot CRC");
  Fixture duplicateAuthId;addIdentity(duplicateAuthId,"D000001","gugy");addIdentity(duplicateAuthId,"D000002","Guest",UserRole::Guest,RecordStatus::Active,20);std::strcpy(duplicateAuthId.auth[1].id,"D000001");build(duplicateAuthId);check(!valid(duplicateAuthId),"duplicate auth IDs");
  Fixture duplicateVerifier;addIdentity(duplicateVerifier,"D000001","gugy");addIdentity(duplicateVerifier,"D000002","Guest",UserRole::Guest,RecordStatus::Active,20);std::memcpy(duplicateVerifier.auth[1].salt,duplicateVerifier.auth[0].salt,sizeof(duplicateVerifier.auth[1].salt));std::memcpy(duplicateVerifier.auth[1].hash,duplicateVerifier.auth[0].hash,sizeof(duplicateVerifier.auth[1].hash));build(duplicateVerifier);check(!valid(duplicateVerifier),"duplicate auth verifier bytes");

  Fixture before;withGuest(before);Fixture changed;withGuest(changed);changed.rows[1].name[0]='X';build(changed);
  check(SecureBackup::sameOwner(before.buffer,changed.buffer),"same Owner survives non-Owner changes");
  Fixture otherOwner;owner(otherOwner);otherOwner.rows[0].name[0]='X';build(otherOwner);
  check(!SecureBackup::sameOwner(baseline.buffer,otherOwner.buffer),"sameOwner rejects Owner name change");
  Fixture otherVerifier;owner(otherVerifier);otherVerifier.auth[0].hash[0]^=0x7f;build(otherVerifier);
  check(!SecureBackup::sameOwner(baseline.buffer,otherVerifier.buffer),"sameOwner rejects Owner verifier change");
  Fixture malformed;owner(malformed);malformed.buffer.size--;
  check(!SecureBackup::sameOwner(baseline.buffer,malformed.buffer),"sameOwner rejects malformed buffer");
  std::printf("snapshot validator tests: %d assertions, %d failures\n",assertions,failures);
  return failures?1:0;
}
