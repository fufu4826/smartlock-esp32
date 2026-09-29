#include "RegistrationRecovery.h"

#include "../storage/AuthStore.h"
#include "../storage/IdentityStore.h"
#include <string.h>

using namespace smartlock::storage;

bool RegistrationRecovery::reconcile(Kind kind, const char* credential,
                                     char (&deviceId)[9], UserRole& role) {
  deviceId[0] = '\0';
  role = UserRole::User;
  if ((kind != Kind::Setup && kind != Kind::Enrollment) || !credential ||
      !IdentityStore::healthy()) return false;

  char matchedId[9] = {};
  if (!AuthStore::findDeviceByCredential(credential, matchedId)) return false;
  IdentityRecord identity = {};
  if (!IdentityStore::find(matchedId, identity) || identity.status != RecordStatus::Active) {
    memset(matchedId, 0, sizeof(matchedId));
    return false;
  }

  const bool owner = strcmp(matchedId, "D000001") == 0 && identity.role == UserRole::Owner;
  if ((kind == Kind::Setup && !owner) ||
      (kind == Kind::Enrollment && (owner || identity.role == UserRole::Owner))) {
    memset(matchedId, 0, sizeof(matchedId));
    return false;
  }
  memcpy(deviceId, matchedId, sizeof(matchedId));
  role = identity.role;
  memset(matchedId, 0, sizeof(matchedId));
  return true;
}
