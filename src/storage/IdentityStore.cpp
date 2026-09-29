#include "IdentityStore.h"
#include "AtomicFileStore.h"
#include "UserStore.h"
#include "DeviceStore.h"
#include "AuthStore.h"
#include <SD.h>
#include <memory>
#include <new>
#include <string.h>

namespace smartlock { namespace storage {
namespace {


const char* backupDir="/smartlock/backups/identity-v1";
const char* intent="IDENTITY_V1_INTENT";
const char* committed="IDENTITY_V1_COMMITTED";
bool artifact(const char* p) {
  char b[100],t[100]; snprintf(b,sizeof(b),"%s.bak",p);snprintf(t,sizeof(t),"%s.tmp",p);
  return SD.exists(p)||SD.exists(b)||SD.exists(t);
}
bool marker(const char* expected) {
  uint8_t bytes[64]={};size_t n=0;
  return AtomicFileStore::read("/smartlock/db/identity-mode.rec",bytes,sizeof(bytes),n)==AtomicFileStore::ReadResult::CurrentValid &&
      n==strlen(expected) && !memcmp(bytes,expected,n);
}
bool setMarker(const char* value) {
  return AtomicFileStore::write("/smartlock/db/identity-mode.rec",reinterpret_cast<const uint8_t*>(value),strlen(value)) && marker(value);
}
bool validId(const char* id) {
  if(!id||strnlen(id,9)!=7||id[0]!='D'||!strcmp(id,"D000000"))return false;
  for(size_t i=1;i<7;++i)if(id[i]<'0'||id[i]>'9')return false;
  return true;
}
bool validate(const IdentityRecord* r,size_t n) {
  if(!r||!n||n>IdentityStore::kMaxRecords)return false;
  size_t owners=0,auth=0;
  if(AuthStore::inspect(auth)!=AuthStore::ReadResult::Valid||auth<n)return false;
  for(size_t i=0;i<n;++i) {
    char name[41]={},line[kMaxRecordLineLength+1];size_t length=0;
    if(!validId(r[i].id)||!IdentityStore::normalizeName(r[i].name,name)||strcmp(name,r[i].name)||
       !encodeIdentityRecord(r[i],line,sizeof(line),&length)||!AuthStore::hasDevice(r[i].id))return false;
    if(r[i].role==UserRole::Owner) {
      if(strcmp(r[i].id,"D000001")||r[i].status!=RecordStatus::Active)return false;
      ++owners;
    } else if(!strcmp(r[i].id,"D000001"))return false;
    for(size_t j=0;j<i;++j)if(!strcmp(r[i].id,r[j].id)||!strcmp(r[i].name,r[j].name))return false;
  }
  return owners==1;
}
bool readRows(IdentityRecord* rows,size_t capacity,size_t& count) {
  count=0;
  std::unique_ptr<uint8_t[]> payload(new(std::nothrow) uint8_t[AtomicFileStore::kMaxPayload]);
  if(!payload||!rows)return false;
  size_t n=0;
  if(AtomicFileStore::read("/smartlock/db/identities.rec",payload.get(),AtomicFileStore::kMaxPayload,n)!=AtomicFileStore::ReadResult::CurrentValid)return false;
  for(size_t start=0;start<n;) {
    size_t end=start;while(end<n&&payload[end]!='\n')++end;
    if(end==n||end==start||count>=capacity||count>=IdentityStore::kMaxRecords||
       !decodeIdentityRecord(reinterpret_cast<const char*>(payload.get()+start),end-start,&rows[count]))return false;
    ++count;start=end+1;
  }
  return validate(rows,count);
}
bool writeRows(const IdentityRecord* rows,size_t count) {
  if(!validate(rows,count))return false;
  std::unique_ptr<uint8_t[]> payload(new(std::nothrow) uint8_t[AtomicFileStore::kMaxPayload]);
  if(!payload)return false;
  size_t used=0;
  for(size_t i=0;i<count;++i) {
    char line[kMaxRecordLineLength+1];size_t n=0;
    if(!encodeIdentityRecord(rows[i],line,sizeof(line),&n)||used+n+1>AtomicFileStore::kMaxPayload)return false;
    memcpy(payload.get()+used,line,n);used+=n;payload[used++]='\n';
  }
  return AtomicFileStore::write("/smartlock/db/identities.rec",payload.get(),used);
}
bool equalFiles(const char* a,const char* b) {
  File left=SD.open(a,FILE_READ),right=SD.open(b,FILE_READ);
  if(!left||!right||left.size()!=right.size())return false;
  uint8_t x[256],y[256];size_t remaining=left.size();
  while(remaining) {
    const size_t n=remaining>sizeof(x)?sizeof(x):remaining;
    if(left.read(x,n)!=n||right.read(y,n)!=n||memcmp(x,y,n))return false;
    remaining-=n;
  }
  return true;
}
bool backupFile(const char* source,const char* destination) {
  if(SD.exists(destination))return false;
  File from=SD.open(source,FILE_READ);
  if(!from||from.size()>AtomicFileStore::kMaxPayload+12)return false;
  File to=SD.open(destination,FILE_WRITE);if(!to)return false;
  uint8_t bytes[256];size_t remaining=from.size();
  while(remaining) {
    const size_t n=remaining>sizeof(bytes)?sizeof(bytes):remaining;
    if(from.read(bytes,n)!=n||to.write(bytes,n)!=n)return false;
    remaining-=n;
  }
  to.flush();to.close();from.close();
  return equalFiles(source,destination);
}
}

bool IdentityStore::normalizeName(const char* input,char (&out)[41]) {
  if(!input)return false;
  size_t n=strnlen(input,128);if(!n||n==128)return false;
  size_t a=0,b=n;
  // ASCII surrounding whitespace is normalized; UTF-8 name validation then
  // rejects interior controls, delimiters, malformed sequences and >40 bytes.
  auto space=[](unsigned char c){return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f';};
  while(a<b&&space(input[a]))++a;
  while(b>a&&space(input[b-1]))--b;
  if(b==a||b-a>40)return false;
  IdentityRecord probe={};strcpy(probe.id,"D000001");memcpy(probe.name,input+a,b-a);
  probe.role=UserRole::Owner;probe.status=RecordStatus::Active;
  char line[kMaxRecordLineLength+1];size_t length=0;
  if(!encodeIdentityRecord(probe,line,sizeof(line),&length))return false;
  memset(out,0,41);memcpy(out,probe.name,b-a);return true;
}
bool IdentityStore::modePresent(){return artifact("/smartlock/db/identity-mode.rec")||artifact("/smartlock/db/identities.rec")||SD.exists(backupDir);}
bool IdentityStore::load(IdentityRecord* rows,size_t cap,size_t& count) {
  count=0;return marker(committed)&&readRows(rows,cap,count);
}
bool IdentityStore::healthy() {
  std::unique_ptr<IdentityRecord[]> rows(new(std::nothrow) IdentityRecord[kMaxRecords]);size_t count=0;
  return rows&&load(rows.get(),kMaxRecords,count);
}
bool IdentityStore::replace(const IdentityRecord* rows,size_t count) {
  return healthy()&&writeRows(rows,count)&&healthy();
}
bool IdentityStore::find(const char* id,IdentityRecord& output) {
  if(!validId(id))return false;
  std::unique_ptr<IdentityRecord[]> rows(new(std::nothrow) IdentityRecord[kMaxRecords]);size_t count=0;
  if(!rows||!load(rows.get(),kMaxRecords,count))return false;
  for(size_t i=0;i<count;++i)if(!strcmp(rows[i].id,id)){output=rows[i];return true;}
  return false;
}
bool IdentityStore::nameAvailable(const char* normalized) {
  std::unique_ptr<IdentityRecord[]> rows(new(std::nothrow) IdentityRecord[kMaxRecords]);size_t count=0;
  if(!normalized||!rows||!load(rows.get(),kMaxRecords,count))return false;
  for(size_t i=0;i<count;++i)if(!strcmp(rows[i].name,normalized))return false;
  return true;
}
bool IdentityStore::migrateLegacy() {
  if(marker(committed))return healthy();
  // An interrupted or missing/corrupt active database is never reconstructed
  // from old Users/Devices: doing so could resurrect revoked credentials.
  if(modePresent())return false;
  std::unique_ptr<UserRecord[]> users(new(std::nothrow) UserRecord[kMaxRecords]);
  std::unique_ptr<DeviceRecord[]> devices(new(std::nothrow) DeviceRecord[kMaxRecords]);
  std::unique_ptr<IdentityRecord[]> identities(new(std::nothrow) IdentityRecord[kMaxRecords]);
  size_t uc=0,dc=0,ac=0,owners=0;
  if(!users||!devices||!identities||!UserStore::load(users.get(),kMaxRecords,uc)||
     !DeviceStore::load(devices.get(),kMaxRecords,dc)||!uc||!dc||
     AuthStore::inspect(ac)!=AuthStore::ReadResult::Valid||ac!=dc)return false;
  for(size_t u=0;u<uc;++u)if(users[u].role==UserRole::Owner) {
    if(strcmp(users[u].id,"U000001")||users[u].status!=RecordStatus::Active)return false;
    ++owners;
  }
  if(owners!=1)return false;
  for(size_t d=0;d<dc;++d) {
    bool found=false;
    for(size_t u=0;u<uc;++u)if(!strcmp(devices[d].userId,users[u].id)) {
      IdentityRecord& r=identities[d];memset(&r,0,sizeof(r));
      snprintf(r.id,sizeof(r.id),"%s",devices[d].id);
      if(!normalizeName(users[u].name,r.name))return false;
      r.role=users[u].role;
      r.status=users[u].status==RecordStatus::Active?devices[d].status:RecordStatus::Revoked;
      found=true;break;
    }
    if(!found)return false;
  }
  if(!validate(identities.get(),dc))return false;
  size_t unbound=0;
  for(size_t u=0;u<uc;++u) {
    bool bound=false;
    for(size_t d=0;d<dc;++d)if(!strcmp(users[u].id,devices[d].userId))bound=true;
    if(!bound)++unbound;
  }
  // Immutable, byte-verified snapshots precede the intent and every new data
  // write. Live auth.rec is NEVER rewritten by this migration.
  if(!SD.mkdir(backupDir)||
     !backupFile("/smartlock/db/users.rec","/smartlock/backups/identity-v1/users.rec")||
     !backupFile("/smartlock/db/devices.rec","/smartlock/backups/identity-v1/devices.rec")||
     !backupFile("/smartlock/db/auth.rec","/smartlock/backups/identity-v1/auth.rec"))return false;
  if(!setMarker(intent)||!writeRows(identities.get(),dc))return false;
  size_t checked=0;
  if(!readRows(identities.get(),kMaxRecords,checked)||checked!=dc||
     !equalFiles("/smartlock/db/auth.rec","/smartlock/backups/identity-v1/auth.rec")||
     !setMarker(committed)||!healthy())return false;
  Serial.printf("IDENTITY MIGRATION: PASS identities=%u archived_unbound_users=%u auth_bytes_unchanged=1\n",(unsigned)dc,(unsigned)unbound);
  return true;
}
bool IdentityStore::createFirst(const char* ownerName) {
  if(modePresent())return false;
  IdentityRecord owner={};strcpy(owner.id,"D000001");owner.role=UserRole::Owner;owner.status=RecordStatus::Active;
  if(!normalizeName(ownerName,owner.name))return false;
  return setMarker(intent)&&writeRows(&owner,1)&&setMarker(committed)&&healthy();
}
bool IdentityStore::backupVerifierUnchanged() {
  return AuthStore::ownerVerifierMatches("/smartlock/backups/identity-v1/auth.rec");
}
} }
