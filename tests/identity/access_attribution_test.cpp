#include "identity_test_support.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace smartlock::storage;
using smartlock::events::Source;
int main() {
  resetIdentityFakes();
  const std::string owner(64,'a'),phone(64,'b');
  UserRecord user={"U000001","Owner",UserRole::Owner,RecordStatus::Active};
  DeviceRecord device={"D000001","U000001","Owner",RecordStatus::Active};
  assert(UserStore::replace(&user,1));assert(DeviceStore::replace(&device,1));
  seedAuth("D000001",owner.c_str());SD.setFileContents("/smartlock/db/auth.rec",verifierImage());
  assert(IdentityStore::migrateLegacy());
  SessionManager sessions;EnrollmentManager manager(sessions);ConfigStore config;LockController lock;
  AccessController access(sessions,lock,config,manager);char qr[65]={},token[65]={},url[128]={},id[9]={};
  assert(sessions.createSession(SessionType::Management,90000,millis(),qr));
  assert(manager.login(qr,"D000001",owner.c_str(),millis(),token));
  assert(manager.createEnrollment("Phone","User",millis(),url));
  assert(manager.completeEnrollment(strstr(url,"session=")+8,phone.c_str(),millis(),id));
  uint32_t duration=0;
  auto fresh=[&](){assert(sessions.createSession(SessionType::Access,30000,millis(),qr));};
  fresh();assert(access.request(qr,"D000001",owner.c_str(),millis(),duration,true)==AccessController::Result::Unlocked);
  assert(!strcmp(Audit::lastDevice(),"D000001")&&Audit::lastSource()==Source::Lan&&duration==5000);
  lock.locked_=true;
  fresh();assert(access.request(qr,"D000001",owner.c_str(),millis(),duration,false)==AccessController::Result::Unlocked);
  assert(Audit::lastSource()==Source::RecoveryAp);lock.locked_=true;
  fresh();assert(access.request(qr,"D000002",phone.c_str(),millis(),duration,false)==AccessController::Result::Denied);
  assert(!strcmp(Audit::lastClaimed(),"D000002")&&!strcmp(Audit::lastDevice(),"D000002")&&Audit::lastSource()==Source::RecoveryAp);
  assert(manager.revoke("D000002"));
  fresh();assert(access.request(qr,"D000002",phone.c_str(),millis(),duration,true)==AccessController::Result::Denied);
  assert(!strcmp(Audit::lastClaimed(),"D000002")&&!strcmp(Audit::lastDevice(),"D000002"));
  fresh();assert(access.request(qr,"D000002",owner.c_str(),millis(),duration,true)==AccessController::Result::Denied);
  assert(!strcmp(Audit::lastClaimed(),"D000002")&&!Audit::lastDevice()[0]);
  fresh();assert(access.request(qr,"D999999",phone.c_str(),millis(),duration,true)==AccessController::Result::Denied);
  assert(!strcmp(Audit::lastClaimed(),"D999999")&&!Audit::lastDevice()[0]);
  sessions.invalidateAllOfType(SessionType::Access);
  assert(access.request(qr,"D000001",owner.c_str(),millis(),duration,true)==AccessController::Result::Denied);
  assert(!Audit::lastDevice()[0]);assert(lock.unlockCalls==2&&lock.isLocked());
  std::cout<<"PASS: 7 production Access attribution scenarios; LAN/recovery, verified revoked, wrong/unknown proof, invalid session, no extra unlock\n";
}
