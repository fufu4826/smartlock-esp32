#include "identity_test_support.h"
#include <cstring>
#include <cstdlib>
#include <iostream>

using namespace smartlock::storage;
namespace {
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": "<<#x<<"\n";std::exit(1);}}while(0)
const std::string ownerCredential(64,'a'),adminCredential(64,'d');

void seed() {
  resetIdentityFakes();
  UserRecord user={"U000001","Owner",UserRole::Owner,RecordStatus::Active};
  DeviceRecord device={"D000001","U000001","Owner browser",RecordStatus::Active};
  CHECK(UserStore::replace(&user,1));CHECK(DeviceStore::replace(&device,1));
  seedAuth("D000001",ownerCredential.c_str());
  SD.setFileContents("/smartlock/db/auth.rec",verifierImage());
  CHECK(IdentityStore::migrateLegacy());
}

bool login(SessionManager& sessions,EnrollmentManager& manager,const char* id,
           const std::string& credential,uint32_t now,char (&token)[65]) {
  setFakeMillis(now);char challenge[65]={};
  if(!sessions.createSession(SessionType::Management,90000,now,challenge))return false;
  return manager.login(challenge,id,credential.c_str(),now,token);
}

void testExpiryAndWrongBearer() {
  seed();SessionManager sessions;EnrollmentManager manager(sessions);char token[65]={};
  const uint32_t start=UINT32_MAX-100u;
  CHECK(login(sessions,manager,"D000001",ownerCredential,start,token));
  CHECK(manager.authorized(token,start+299999u)); // just before expiry, across millis wrap
  SessionManager rebootSessions;EnrollmentManager rebootManager(rebootSessions);
  CHECK(!rebootManager.authorized(token,start+1u)); // reboot leaves no temporary ManagementAuth slot
  char wrong[65];std::memset(wrong,'f',64);wrong[64]=0;
  CHECK(!manager.authorized(wrong,start+299999u));
  CHECK(manager.authorized(token,start+299999u)); // bogus auth must not log out the real administrator
  const uint32_t expiry=start+300000u;
  CHECK(!manager.authorized(token,expiry));
  CHECK(!manager.authorized(token,expiry+1u));
  // Production loop calls expireSessions continuously. Once expiry is observed,
  // the same uint32 timestamp one complete wrap later still has no live slot.
  uint64_t afterExpiry=static_cast<uint64_t>(start)+300000u;
  sessions.expireSessions(static_cast<uint32_t>(afterExpiry));
  CHECK(!manager.authorized(token,static_cast<uint32_t>(afterExpiry)));
  uint64_t afterOneFullWrap=afterExpiry;
  afterOneFullWrap+=(uint64_t(1)<<32);
  uint32_t wrappedExpiry=static_cast<uint32_t>(afterOneFullWrap);
  uint32_t firstExpiry=static_cast<uint32_t>(afterExpiry);
  CHECK(wrappedExpiry==firstExpiry);
  sessions.expireSessions(wrappedExpiry);
  CHECK(!sessions.validateSession(SessionType::ManagementAuth,token,wrappedExpiry));
  uint64_t oneWrapPlusOne=static_cast<uint64_t>(start)+(uint64_t(1)<<32)+1u;
  const uint32_t reusedEarlyTimestamp=static_cast<uint32_t>(oneWrapPlusOne);
  CHECK(reusedEarlyTimestamp==start+1u);
  CHECK(!manager.authorized(token,reusedEarlyTimestamp));
  sessions.expireSessions(reusedEarlyTimestamp);
  CHECK(!sessions.validateSession(SessionType::ManagementAuth,token,reusedEarlyTimestamp));
}

void testReplacementAndRevokedIssuer() {
  seed();SessionManager sessions;EnrollmentManager manager(sessions);
  char oldToken[65]={},newToken[65]={};
  CHECK(login(sessions,manager,"D000001",ownerCredential,10,oldToken));
  CHECK(login(sessions,manager,"D000001",ownerCredential,20,newToken));
  CHECK(!manager.authorized(oldToken,21));CHECK(manager.authorized(newToken,21));

  char url[128]={};
  setFakeMillis(22);CHECK(manager.createEnrollment("Admin","Admin",22,url));
  const char* marker=std::strstr(url,"session=");CHECK(marker);
  char id[9]={};setFakeMillis(23);
  CHECK(manager.completeEnrollment(marker+8,adminCredential.c_str(),23,id));
  CHECK(!std::strcmp(id,"D000002"));
  char adminToken[65]={};CHECK(login(sessions,manager,id,adminCredential,30,adminToken));

  IdentityRecord rows[IdentityStore::kMaxRecords]={};size_t count=0;
  CHECK(IdentityStore::load(rows,IdentityStore::kMaxRecords,count));
  rows[1].status=RecordStatus::Revoked;CHECK(IdentityStore::replace(rows,count));
  CHECK(!manager.authorized(adminToken,31));
  rows[1].status=RecordStatus::Active;CHECK(IdentityStore::replace(rows,count));
  CHECK(!manager.authorized(adminToken,32)); // issuer reactivation cannot revive the retired slot
}
}

int main(){
  testExpiryAndWrongBearer();
  testReplacementAndRevokedIssuer();
  std::cout<<"PASS: "<<checks<<" ManagementAuth assertions (expiry boundaries, wrap, replacement, issuer revocation)\n";
}
