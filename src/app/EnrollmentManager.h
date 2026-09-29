#pragma once
#include <Arduino.h>
#include "../qr/SessionManager.h"
#include "../storage/RecordCodec.h"
// Single loop authority; enrollment cannot reach the lock output.
class EnrollmentManager {
 public:
  explicit EnrollmentManager(SessionManager& s) : sessions_(s) {}
  bool login(const char*,const char*,const char*,uint32_t,char (&)[65]);
  bool authorized(const char*,uint32_t);
  bool authorizedOwner(const char* t,uint32_t n) { return authorized(t,n)&&adminRole_==smartlock::storage::UserRole::Owner; }
  bool snapshot(String&);
  const char* actorRole() const;
  bool enrollmentInfo(const char*,uint32_t,String&);
  bool createEnrollment(const char* name,const char* role,uint32_t,char (&)[128]);
  bool nameTaken() const { return nameTaken_; }
  const char* credentialStatus(const char* session,const char* id,const char* credential,uint32_t now);
  bool checkEnrollment(const char*,uint32_t);
  bool completeEnrollment(const char*,const char*,uint32_t,char (&)[9]);
  bool revoke(const char*);
  bool databaseIntegrity(size_t& identities,size_t& owners,size_t& active,size_t& revoked);
  bool isActiveOwnerDevice(const char*);
 private:
  bool activeAdminDevice(const char*);
  SessionManager& sessions_;
  char adminToken_[65]={},adminDevice_[9]={};
  smartlock::storage::UserRole adminRole_=smartlock::storage::UserRole::User;
  char pendingName_[41]={},pendingIssuer_[9]={};
  smartlock::storage::UserRole pendingRole_=smartlock::storage::UserRole::User;
  uint32_t enrollmentCreated_=0;
  bool nameTaken_=false;
};
