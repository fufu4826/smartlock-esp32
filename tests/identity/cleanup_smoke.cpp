#include "identity_test_support.h"
#include "AtomicFileStore.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace smartlock::storage;
int main(){
 resetIdentityFakes();LineNotifications::emitted=0;const std::string cred(64,'a');
 UserRecord user={"U000001","gugy",UserRole::Owner,RecordStatus::Active};DeviceRecord device={"D000001","U000001","Owner",RecordStatus::Active};
 assert(UserStore::replace(&user,1));assert(DeviceStore::replace(&device,1));seedAuth("D000001",cred.c_str());SD.setFileContents("/smartlock/db/auth.rec",verifierImage());
 assert(IdentityStore::migrateLegacy());assert(IdentityStore::healthy());assert(IdentityStore::backupVerifierUnchanged());
 SessionManager sessions;EnrollmentManager manager(sessions);ConfigStore config;LockController lock;AccessController access(sessions,lock,config,manager);char qr[65]={},token[65]={};uint32_t duration=0;
 assert(sessions.createSession(SessionType::Access,30000,millis(),qr));
 assert(access.request(qr,"D000001",cred.c_str(),millis(),duration,true)==AccessController::Result::Unlocked);assert(duration==5000&&lock.unlockCalls==1&&LineNotifications::emitted==1);
 assert(access.request(qr,"D000001",cred.c_str(),millis(),duration,true)==AccessController::Result::Denied);assert(lock.unlockCalls==1);
 assert(sessions.createSession(SessionType::Access,30000,millis(),qr));assert(access.request(qr,"D000001",cred.c_str(),millis(),duration,true)==AccessController::Result::AlreadyUnlocked);assert(LineNotifications::emitted==1);
 lock.locked_=true;assert(sessions.createSession(SessionType::Access,30000,millis(),qr));assert(access.request(qr,"D000001",std::string(64,'b').c_str(),millis(),duration,true)==AccessController::Result::Denied);assert(lock.unlockCalls==1&&LineNotifications::emitted==1);
 assert(sessions.createSession(SessionType::Management,90000,millis(),qr));assert(manager.login(qr,"D000001",cred.c_str(),millis(),token));assert(manager.authorizedOwner(token,millis()));assert(lock.unlockCalls==1);
 IdentityRecord owner={};assert(IdentityStore::find("D000001",owner));assert(owner.role==UserRole::Owner&&owner.status==RecordStatus::Active);

 char url[128]={},id[9]={};assert(manager.createEnrollment("Fresh Phone","User",millis(),url));const std::string invite=strstr(url,"session=")+8;const uint32_t inviteAt=millis();
 assert(!strcmp(manager.credentialStatus(invite.c_str(),"D000001",cred.c_str(),millis()),"ACTIVE"));
 assert(!strcmp(manager.credentialStatus(invite.c_str(),"D000001",std::string(64,'b').c_str(),millis()),"UNKNOWN"));
 assert(!strcmp(manager.credentialStatus(invite.c_str(),"D999999",cred.c_str(),millis()),"UNKNOWN"));
 setFakeMillis(inviteAt+120001);assert(manager.credentialStatus(invite.c_str(),"D000001",cred.c_str(),millis())==nullptr);
 assert(!manager.completeEnrollment(invite.c_str(),std::string(64,'c').c_str(),millis(),id));
 const std::string phone(64,'c');assert(manager.createEnrollment("Fresh Phone Retry","User",millis(),url));const std::string validInvite=strstr(url,"session=")+8;
 assert(manager.completeEnrollment(validInvite.c_str(),phone.c_str(),millis(),id));assert(!strcmp(id,"D000002"));
 assert(!manager.completeEnrollment(invite.c_str(),phone.c_str(),millis(),id));assert(!manager.credentialStatus(invite.c_str(),"D000001",cred.c_str(),millis()));
 assert(manager.revoke("D000002"));assert(manager.createEnrollment("Phone Again","User",millis(),url));const std::string again=strstr(url,"session=")+8;
 assert(!strcmp(manager.credentialStatus(again.c_str(),"D000002",phone.c_str(),millis()),"REVOKED"));
 assert(manager.completeEnrollment(again.c_str(),std::string(64,'d').c_str(),millis(),id));assert(!strcmp(id,"D000003"));
 IdentityRecord revoked={};assert(IdentityStore::find("D000002",revoked)&&revoked.status==RecordStatus::Revoked);
 assert(AuthStore::verifyDevice("D000001",cred.c_str()));assert(lock.unlockCalls==1&&lock.isLocked());

 IdentityRecord rows[64]={};size_t count=0;assert(IdentityStore::load(rows,64,count));rows[2].role=UserRole::Admin;assert(IdentityStore::replace(rows,count));
 assert(sessions.createSession(SessionType::Management,90000,millis(),qr));assert(manager.login(qr,"D000003",std::string(64,'d').c_str(),millis(),token));
 assert(manager.createEnrollment("Issuer Invalid","User",millis(),url));const std::string invalid=strstr(url,"session=")+8;
 rows[2].status=RecordStatus::Revoked;assert(IdentityStore::replace(rows,count));
 assert(!manager.credentialStatus(invalid.c_str(),"D000002",phone.c_str(),millis()));assert(!manager.completeEnrollment(invalid.c_str(),std::string(64,'e').c_str(),millis(),id));
 config.setHealthy(false);assert(sessions.createSession(SessionType::Access,30000,millis(),qr));assert(access.request(qr,"D000001",cred.c_str(),millis(),duration,true)==AccessController::Result::StorageFault);assert(lock.unlockCalls==1);
 std::cout<<"Cleanup core Access/Management/storage smoke PASS (isolated fakes)\n";
}
