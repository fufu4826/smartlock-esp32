#include "EnrollmentManager.h"
#include "../storage/IdentityStore.h"
#include "../storage/AuthStore.h"
#include "../network/CanonicalOrigin.h"
#include <string.h>
using namespace smartlock::storage;
namespace {
IdentityRecord identities[IdentityStore::kMaxRecords];
bool goodToken(const char* s) {
  if(!s||strnlen(s,65)!=64)return false;
  for(size_t i=0;i<64;++i)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;
  return true;
}
bool sameToken(const char* a,const char* b) {
  if(!goodToken(a)||!goodToken(b))return false;
  unsigned char diff=0;for(size_t i=0;i<64;++i)diff|=a[i]^b[i];return diff==0;
}
const char* roleName(UserRole r) {
  switch(r){case UserRole::Owner:return "Owner";case UserRole::Admin:return "Admin";
    case UserRole::Guest:return "Guest";default:return "User";}
}
bool mayGrant(UserRole issuer,UserRole target) {
  return (issuer==UserRole::Owner&&target!=UserRole::Owner)||
    (issuer==UserRole::Admin&&(target==UserRole::User||target==UserRole::Guest));
}
void appendJsonString(String& out,const char* s) {
  out+='"';for(const unsigned char* p=(const unsigned char*)s;*p;++p){
    if(*p=='"'||*p=='\\')out+='\\';if(*p>=0x20)out+=(char)*p;
  }out+='"';
}
}
bool EnrollmentManager::activeAdminDevice(const char* id) {
  IdentityRecord r={};
  if(!IdentityStore::find(id,r)||r.status!=RecordStatus::Active||
     (r.role!=UserRole::Owner&&r.role!=UserRole::Admin))return false;
  adminRole_=r.role;return true;
}
bool EnrollmentManager::isActiveOwnerDevice(const char* id) {
  IdentityRecord r={};return IdentityStore::find(id,r)&&r.status==RecordStatus::Active&&r.role==UserRole::Owner;
}
bool EnrollmentManager::login(const char* session,const char* id,const char* credential,uint32_t now,char (&token)[65]) {
  if(!sessions_.validateSession(SessionType::Management,session,now)||!activeAdminDevice(id)||
     !AuthStore::verifyDevice(id,credential)||!sessions_.consumeSession(SessionType::Management,session,millis()))return false;
  if(!sessions_.createSession(SessionType::ManagementAuth,300000,now,token))return false;
  memcpy(adminToken_,token,65);snprintf(adminDevice_,sizeof(adminDevice_),"%s",id);
  return true;
}
bool EnrollmentManager::authorized(const char* token,uint32_t now) {
  sessions_.expireSessions(now);
  if(!goodToken(adminToken_))return false;
  if(!sessions_.validateSession(SessionType::ManagementAuth,adminToken_,now)||!activeAdminDevice(adminDevice_)){
    sessions_.invalidateAllOfType(SessionType::ManagementAuth);
    memset(adminToken_,0,sizeof(adminToken_));memset(adminDevice_,0,sizeof(adminDevice_));
    adminRole_=UserRole::User;return false;
  }
  return sameToken(adminToken_,token);
}
const char* EnrollmentManager::actorRole() const{return roleName(adminRole_);}
bool EnrollmentManager::snapshot(String& json) {
  size_t count=0;if(!IdentityStore::load(identities,IdentityStore::kMaxRecords,count))return false;
  json="{\"identities\":[";
  for(size_t i=0;i<count;++i){if(i)json+=',';json+="{\"id\":";appendJsonString(json,identities[i].id);
    json+=",\"name\":";appendJsonString(json,identities[i].name);
    json+=",\"role\":";appendJsonString(json,roleName(identities[i].role));
    json+=",\"status\":\"";json+=identities[i].status==RecordStatus::Active?"Active":"Revoked";json+="\"}";
  }json+="]}";return true;
}
bool EnrollmentManager::createEnrollment(const char* name,const char* role,uint32_t now,char (&url)[128]) {
  nameTaken_=false;char normalized[41]={};
  if(!authorized(adminToken_,now)||!role||!IdentityStore::normalizeName(name,normalized))return false;
  UserRole target;
  if(!strcmp(role,"Admin"))target=UserRole::Admin;
  else if(!strcmp(role,"User"))target=UserRole::User;
  else if(!strcmp(role,"Guest"))target=UserRole::Guest;
  else return false;
  if(!mayGrant(adminRole_,target))return false;
  size_t count=0;if(!IdentityStore::load(identities,IdentityStore::kMaxRecords,count)||count>=IdentityStore::kMaxRecords)return false;
  for(size_t i=0;i<count;++i)if(!strcmp(normalized,identities[i].name)){nameTaken_=true;return false;}
  char token[65]={};if(!sessions_.createSession(SessionType::Enrollment,120000,now,token))return false;
  memcpy(pendingName_,normalized,sizeof(pendingName_));pendingRole_=target;
  snprintf(pendingIssuer_,sizeof(pendingIssuer_),"%s",adminDevice_);enrollmentCreated_=now;
  snprintf(url,128,"http://%s/enroll?session=%s",CanonicalOrigin::host(),token);memset(token,0,sizeof(token));return true;
}
bool EnrollmentManager::checkEnrollment(const char* session,uint32_t now) {
  if(!pendingName_[0]||!sessions_.validateSession(SessionType::Enrollment,session,now))return false;
  IdentityRecord issuer={};
  return IdentityStore::find(pendingIssuer_,issuer)&&issuer.status==RecordStatus::Active&&
      mayGrant(issuer.role,pendingRole_)&&IdentityStore::nameAvailable(pendingName_);
}
const char* EnrollmentManager::credentialStatus(const char* session,const char* id,const char* credential,uint32_t now) {
  // The invitation capability is checked before inspecting any candidate.
  if(!checkEnrollment(session,now)||!IdentityStore::healthy())return nullptr;
  IdentityRecord record={};
  if(!id||!goodToken(credential)||!IdentityStore::find(id,record)||
     !AuthStore::verifyDevice(id,credential))return "UNKNOWN";
  return record.status==RecordStatus::Active?"ACTIVE":"REVOKED";
}
bool EnrollmentManager::enrollmentInfo(const char* session,uint32_t now,String& json) {
  if(!checkEnrollment(session,now))return false;
  json="{\"ok\":true,\"name\":";appendJsonString(json,pendingName_);
  json+=",\"role\":";appendJsonString(json,roleName(pendingRole_));
  json+=",\"expiresIn\":";json+=String((120000-uint32_t(now-enrollmentCreated_))/1000);json+='}';return true;
}
bool EnrollmentManager::completeEnrollment(const char* session,const char* credential,uint32_t now,char (&id)[9]) {
  if(!goodToken(credential)||!checkEnrollment(session,now))return false;
  size_t count=0;if(!IdentityStore::load(identities,IdentityStore::kMaxRecords,count)||count>=IdentityStore::kMaxRecords)return false;
  unsigned largest=0;
  for(size_t i=0;i<count;++i){unsigned number=0;for(size_t j=1;j<7;++j)number=number*10+identities[i].id[j]-'0';if(number>largest)largest=number;}
  do {if(largest>=999999)return false;snprintf(id,9,"D%06u",++largest);}while(AuthStore::hasDevice(id));
  IdentityRecord candidate={};memcpy(candidate.id,id,9);memcpy(candidate.name,pendingName_,41);
  candidate.role=pendingRole_;candidate.status=RecordStatus::Active;
  // Consume before any credential write; persistence failures cannot replay grants.
  if(!sessions_.consumeSession(SessionType::Enrollment,session,millis()))return false;
  memset(pendingName_,0,sizeof(pendingName_));memset(pendingIssuer_,0,sizeof(pendingIssuer_));
  // Orphan verifiers after power loss have no identity and cannot authorize.
  if(!AuthStore::addDevice(id,credential))return false;
  identities[count]=candidate;
  if(!IdentityStore::replace(identities,count+1)||!AuthStore::verifyDevice(id,credential))return false;
  return true;
}
bool EnrollmentManager::revoke(const char* id) {
  if(!id||!strcmp(id,"D000001")||!authorized(adminToken_,millis()))return false;
  size_t count=0;if(!IdentityStore::load(identities,IdentityStore::kMaxRecords,count))return false;
  for(size_t i=0;i<count;++i)if(!strcmp(identities[i].id,id)&&identities[i].status==RecordStatus::Active){
    if(!mayGrant(adminRole_,identities[i].role))return false;
    identities[i].status=RecordStatus::Revoked;
    if(!IdentityStore::replace(identities,count))return false;
    return true;
  }return false;
}
bool EnrollmentManager::databaseIntegrity(size_t& count,size_t& owners,size_t& active,size_t& revoked) {
  count=owners=active=revoked=0;
  if(!IdentityStore::load(identities,IdentityStore::kMaxRecords,count))return false;
  for(size_t i=0;i<count;++i){if(identities[i].role==UserRole::Owner)++owners;
    if(identities[i].status==RecordStatus::Active)++active;else ++revoked;}
  return owners==1;
}
