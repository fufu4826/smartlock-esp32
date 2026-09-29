#include "identity_test_support.h"
#include "AtomicFileStore.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
using namespace smartlock::storage;
namespace {
unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": "<<#x<<"\n";std::exit(1);}}while(0)
const std::string ownerCredential(64,'a'),phone2Credential(64,'b'),phone3Credential(64,'c'),adminCredential(64,'d');
void seedLegacy() {
  resetIdentityFakes();
  UserRecord users[2]={{"U000001","gugy",UserRole::Owner,RecordStatus::Active},{"U000002","Boom",UserRole::User,RecordStatus::Active}};
  DeviceRecord device={"D000001","U000001","Owner browser",RecordStatus::Active};
  CHECK(UserStore::replace(users,2));CHECK(DeviceStore::replace(&device,1));
  seedAuth("D000001",ownerCredential.c_str());SD.setFileContents("/smartlock/db/auth.rec",verifierImage());
}
std::string session(SessionManager& sessions,SessionType type,uint32_t ttl=120000) {
  char out[65]={};CHECK(sessions.createSession(type,ttl,millis(),out));return out;
}
void login(EnrollmentManager& manager,SessionManager& sessions,const char* id,const std::string& credential) {
  char token[65]={};const auto qr=session(sessions,SessionType::Management);
  CHECK(manager.login(qr.c_str(),id,credential.c_str(),millis(),token));CHECK(manager.authorized(token,millis()));CHECK(manager.authorizedOwner(token,millis())==!strcmp(id,"D000001"));
}
std::string grant(EnrollmentManager& manager,const char* name,const char* role) {
  char url[128]={};CHECK(manager.createEnrollment(name,role,millis(),url));
  const char* token=std::strstr(url,"session=");CHECK(token);return token+8;
}
std::string enroll(EnrollmentManager& manager,const char* name,const char* role,const std::string& credential) {
  const auto qr=grant(manager,name,role);char id[9]={};
  CHECK(manager.completeEnrollment(qr.c_str(),credential.c_str(),millis(),id));
  CHECK(!manager.completeEnrollment(qr.c_str(),credential.c_str(),millis(),id));return id;
}
void ownerInvariant() {
  IdentityRecord owner={};CHECK(IdentityStore::find("D000001",owner));
  CHECK(!strcmp(owner.name,"gugy"));CHECK(owner.role==UserRole::Owner);CHECK(owner.status==RecordStatus::Active);
  CHECK(AuthStore::verifyDevice("D000001",ownerCredential.c_str()));CHECK(IdentityStore::backupVerifierUnchanged());
}
}
int main() {
  // Web request creates a session after loop-start; an old timestamp underflows.
  {
    SessionManager clockTest;char token[65]={};setFakeMillis(100);
    const uint32_t loopStart=millis();setFakeMillis(125);
    CHECK(clockTest.createSession(SessionType::Enrollment,120000,millis(),token));
    CHECK(clockTest.validateSession(SessionType::Enrollment,token,millis()));
    clockTest.expireSessions(loopStart);
    CHECK(!clockTest.validateSession(SessionType::Enrollment,token,millis())); // reproduced old loop
    CHECK(clockTest.createSession(SessionType::Enrollment,120000,millis(),token));
    clockTest.expireSessions(millis()); // fixed production loop call
    CHECK(clockTest.validateSession(SessionType::Enrollment,token,millis()));
    clockTest.expireSessions(120124);CHECK(clockTest.validateSession(SessionType::Enrollment,token,120124));
    clockTest.expireSessions(120125);CHECK(!clockTest.validateSession(SessionType::Enrollment,token,120125));
  }

  seedLegacy();const auto authBefore=SD.fileContents("/smartlock/db/auth.rec");
  const auto usersBefore=SD.fileContents("/smartlock/db/users.rec"),devicesBefore=SD.fileContents("/smartlock/db/devices.rec");
  CHECK(IdentityStore::migrateLegacy());ownerInvariant();
  CHECK(SD.fileContents("/smartlock/db/auth.rec")==authBefore);
  CHECK(SD.fileContents("/smartlock/backups/identity-v1/users.rec")==usersBefore);
  CHECK(SD.fileContents("/smartlock/backups/identity-v1/devices.rec")==devicesBefore);
  CHECK(SD.fileContents("/smartlock/db/users.rec")==usersBefore);
  IdentityRecord rows[64]={};size_t count=0;
  CHECK(IdentityStore::load(rows,64,count)&&count==1);CHECK(IdentityStore::nameAvailable("Boom"));
  // Normal boot is read-only and no longer depends on old relational files.
  const auto image=SD.fileContents("/smartlock/db/identities.rec");
  CHECK(IdentityStore::migrateLegacy());CHECK(SD.fileContents("/smartlock/db/identities.rec")==image);
  SD.setFileContents("/smartlock/db/users.rec","corrupt legacy file");
  SD.remove("/smartlock/db/devices.rec");CHECK(IdentityStore::healthy());ownerInvariant();
  char name[41]={};CHECK(IdentityStore::normalizeName(" \tBoom\r\n",name)&&!strcmp(name,"Boom"));
  CHECK(!IdentityStore::normalizeName(" \t ",name));CHECK(!IdentityStore::normalizeName("a|b",name));
  CHECK(!IdentityStore::normalizeName("a\nb",name));CHECK(!IdentityStore::normalizeName("\xc0\xaf",name));
  CHECK(!IdentityStore::normalizeName(std::string(41,'x').c_str(),name));
  CHECK(IdentityStore::normalizeName("ชื่อ",name));
  SessionManager sessions;EnrollmentManager manager(sessions);ConfigStore config;LockController lock;
  AccessController access(sessions,lock,config,manager);uint32_t duration=0;
  char url[128]={},id[9]={};CHECK(!manager.createEnrollment("No auth","User",millis(),url));
  login(manager,sessions,"D000001",ownerCredential);
  CHECK(!manager.createEnrollment("gugy","User",millis(),url)&&manager.nameTaken());
  CHECK(!manager.createEnrollment("   gugy \t","User",millis(),url)&&manager.nameTaken());
  CHECK(!manager.createEnrollment("Second owner","Owner",millis(),url));
  const auto pending=grant(manager,"  Boom  ","User");
  CHECK(IdentityStore::load(rows,64,count)&&count==1);
  String info;CHECK(manager.enrollmentInfo(pending.c_str(),millis(),info));
  CHECK(std::strstr(info.c_str(),"\"name\":\"Boom\""));
  CHECK(!manager.completeEnrollment(pending.c_str(),"bad credential",millis(),id));
  CHECK(manager.completeEnrollment(pending.c_str(),phone2Credential.c_str(),millis(),id));CHECK(!strcmp(id,"D000002"));
  CHECK(!manager.checkEnrollment(pending.c_str(),millis()));
  CHECK(!manager.completeEnrollment(pending.c_str(),phone3Credential.c_str(),millis(),id));
  const auto third=enroll(manager,"Third","Guest",phone3Credential);CHECK(third=="D000003");
  CHECK(!AuthStore::verifyDevice("D000002",phone3Credential.c_str()));ownerInvariant();
  size_t identities=0,owners=0,active=0,revoked=0;
  CHECK(manager.databaseIntegrity(identities,owners,active,revoked));CHECK(identities==3&&owners==1&&active==3&&revoked==0);
  CHECK(lock.unlockCalls==0);
  auto qr=session(sessions,SessionType::Access);
  CHECK(access.request(qr.c_str(),"D000002",phone2Credential.c_str(),millis(),duration,false)==AccessController::Result::Denied);
  CHECK(lock.unlockCalls==0);
  CHECK(access.request(qr.c_str(),"D000002",phone2Credential.c_str(),millis(),duration,true)==AccessController::Result::Unlocked);
  CHECK(duration==5000&&lock.unlockCalls==1);CHECK(!strcmp(Audit::lastUser(),"")&&!strcmp(Audit::lastDevice(),"D000002"));
  lock.locked_=true;CHECK(access.request(qr.c_str(),"D000002",phone2Credential.c_str(),millis(),duration,true)==AccessController::Result::Denied);
  CHECK(manager.revoke("D000002"));CHECK(!manager.revoke("D000001"));ownerInvariant();
  CHECK(!manager.createEnrollment(" Boom ","User",millis(),url)&&manager.nameTaken());
  // Persistent revocation survives new manager/session objects (simulated restart).
  SessionManager rebootSessions;EnrollmentManager rebootManager(rebootSessions);
  AccessController rebootAccess(rebootSessions,lock,config,rebootManager);
  qr=session(rebootSessions,SessionType::Access);
  CHECK(rebootAccess.request(qr.c_str(),"D000002",phone2Credential.c_str(),millis(),duration,true)==AccessController::Result::Denied);
  CHECK(rebootManager.databaseIntegrity(identities,owners,active,revoked));CHECK(identities==3&&owners==1&&active==2&&revoked==1);
  CHECK(IdentityStore::find("D000003",rows[0])&&rows[0].status==RecordStatus::Active);
  qr=session(rebootSessions,SessionType::Access);
  CHECK(rebootAccess.request(qr.c_str(),"D000001",ownerCredential.c_str(),millis(),duration,true)==AccessController::Result::Unlocked);
  lock.locked_=true;
  // Unknown browser, expired and wrong-type access never unlock.
  qr=session(rebootSessions,SessionType::Management);
  CHECK(rebootAccess.request(qr.c_str(),"D000001",ownerCredential.c_str(),millis(),duration,true)==AccessController::Result::Denied);
  qr=session(rebootSessions,SessionType::Access,1);setFakeMillis(millis()+1);
  CHECK(rebootAccess.request(qr.c_str(),"D000001",ownerCredential.c_str(),millis(),duration,true)==AccessController::Result::Denied);
  qr=session(rebootSessions,SessionType::Access);
  CHECK(rebootAccess.request(qr.c_str(),"D999999",ownerCredential.c_str(),millis(),duration,true)==AccessController::Result::Denied);
  rebootAccess.suspend();
  CHECK(!rebootSessions.validateSession(SessionType::Access,qr.c_str(),millis()));
  qr=session(rebootSessions,SessionType::Access);
  config.setHealthy(false);
  CHECK(rebootAccess.request(qr.c_str(),"D000001",ownerCredential.c_str(),millis(),duration,true)==AccessController::Result::StorageFault);config.setHealthy(true);
  login(manager,sessions,"D000001",ownerCredential);
  auto expiring=grant(manager,"Expires","User");setFakeMillis(millis()+120000);
  CHECK(!manager.completeEnrollment(expiring.c_str(),phone3Credential.c_str(),millis(),id));
  auto replaced=grant(manager,"Old grant","User");auto current=grant(manager,"New grant","User");
  CHECK(!manager.checkEnrollment(replaced.c_str(),millis()));CHECK(manager.checkEnrollment(current.c_str(),millis()));
  // Reusing an existing credential cannot create a new identity; token still consumed.
  CHECK(!manager.completeEnrollment(current.c_str(),ownerCredential.c_str(),millis(),id));CHECK(!manager.checkEnrollment(current.c_str(),millis()));
  CHECK(IdentityStore::load(rows,64,count)&&count==3);
  const auto admin=enroll(manager,"Administrator","Admin",adminCredential);
  login(manager,sessions,admin.c_str(),adminCredential);
  CHECK(!manager.createEnrollment("Escalate","Admin",millis(),url));CHECK(!manager.createEnrollment("Escalate","Owner",millis(),url));
  CHECK(!manager.revoke("D000001"));CHECK(!manager.revoke(admin.c_str()));
  auto issuerGrant=grant(manager,"Issuer revoked","User");
  login(manager,sessions,"D000001",ownerCredential);CHECK(manager.revoke(admin.c_str()));
  CHECK(!manager.completeEnrollment(issuerGrant.c_str(),std::string(64,'e').c_str(),millis(),id));
  char loginToken[65]={};qr=session(sessions,SessionType::Management);
  CHECK(!manager.login(qr.c_str(),"D000003",phone3Credential.c_str(),millis(),loginToken));
  CHECK(lock.unlockCalls==2);ownerInvariant();
  // The committed database cannot be resurrected from old files or backups.
  const auto validIdentityImage=SD.fileContents("/smartlock/db/identities.rec");
  SD.setFileContents("/smartlock/db/identities.rec","corrupt identity CRC");
  CHECK(!IdentityStore::healthy());CHECK(!IdentityStore::migrateLegacy());
  SD.setFileContents("/smartlock/db/identities.rec",validIdentityImage);CHECK(IdentityStore::healthy());
  SD.remove("/smartlock/db/identities.rec");CHECK(!IdentityStore::healthy());CHECK(!IdentityStore::migrateLegacy());
  CHECK(!IdentityStore::createFirst("replacement"));
  // Ambiguous duplicate Owner devices and normalized names stop before backup.
  seedLegacy();DeviceRecord duplicate[2]={{"D000001","U000001","Owner",RecordStatus::Active},{"D000002","U000001","Other",RecordStatus::Active}};
  seedAuth("D000002",phone2Credential.c_str());CHECK(DeviceStore::replace(duplicate,2));
  CHECK(!IdentityStore::migrateLegacy());CHECK(!IdentityStore::modePresent());
  seedLegacy();UserRecord collision[2]={{"U000001","gugy",UserRole::Owner,RecordStatus::Active},{"U000002"," gugy ",UserRole::User,RecordStatus::Active}};
  CHECK(UserStore::replace(collision,2));std::strcpy(duplicate[1].userId,"U000002");
  seedAuth("D000002",phone2Credential.c_str());CHECK(DeviceStore::replace(duplicate,2));
  CHECK(!IdentityStore::migrateLegacy());CHECK(!IdentityStore::modePresent());
  // Exercise every SD mutation boundary in the migration, preserving old authority.
  seedLegacy();const size_t before=SD.mutationCount();CHECK(IdentityStore::migrateLegacy());
  const size_t operations=SD.mutationCount()-before;CHECK(operations>10);
  for(size_t failure=0;failure<operations;++failure) {
    seedLegacy();const auto u=SD.fileContents("/smartlock/db/users.rec"),d=SD.fileContents("/smartlock/db/devices.rec"),a=SD.fileContents("/smartlock/db/auth.rec");
    SD.failAfterMutations(failure);CHECK(!IdentityStore::migrateLegacy());CHECK(SD.faultTriggered());
    CHECK(SD.fileContents("/smartlock/db/users.rec")==u);CHECK(SD.fileContents("/smartlock/db/devices.rec")==d);CHECK(SD.fileContents("/smartlock/db/auth.rec")==a);
    CHECK(!IdentityStore::healthy());
    if(IdentityStore::modePresent())CHECK(!IdentityStore::migrateLegacy());
    else CHECK(IdentityStore::migrateLegacy());
  }
  std::cout<<"PASS: "<<checks<<" assertions, "<<operations<<" migration fault boundaries; migration, authorization, enrollment, replay, role restrictions, revocation and Access fail-closed\n";
}
