#include "RegistrationRecovery.h"
#include "AuthStore.h"
#include "IdentityStore.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <cstdio>

using smartlock::storage::IdentityRecord;
using smartlock::storage::RecordStatus;
using smartlock::storage::UserRole;

namespace {
bool databaseHealthy = true;
IdentityRecord identities[3] = {};
size_t identityCount = 0;
struct Proof { const char* credential; const char* id; };
const Proof proofs[] = {{"a", "D000001"}, {"b", "D000002"},
                        {"c", "D000003"}, {"d", "D000099"}};
}

bool AuthStore::findDeviceByCredential(const char* credential, char (&id)[9]) {
  id[0] = '\0';
  if (!credential) return false;
  for (const auto& proof : proofs) if (!strcmp(credential, proof.credential)) {
    snprintf(id, 9, "%s", proof.id);
    return true;
  }
  return false;
}

bool smartlock::storage::IdentityStore::healthy() { return databaseHealthy; }
bool smartlock::storage::IdentityStore::find(const char* id, IdentityRecord& record) {
  if (!id) return false;
  for (size_t i = 0; i < identityCount; ++i) if (!strcmp(id, identities[i].id)) {
    record = identities[i];
    return true;
  }
  return false;
}

static void setIdentity(size_t i, const char* id, UserRole role, RecordStatus status) {
  snprintf(identities[i].id, sizeof(identities[i].id), "%s", id);
  identities[i].role = role;
  identities[i].status = status;
}

int main() {
  identityCount = 3;
  setIdentity(0, "D000001", UserRole::Owner, RecordStatus::Active);
  setIdentity(1, "D000002", UserRole::User, RecordStatus::Active);
  setIdentity(2, "D000003", UserRole::Guest, RecordStatus::Revoked);
  char id[9] = {};
  UserRole role = UserRole::Admin;

  assert(RegistrationRecovery::reconcile(RegistrationRecovery::Kind::Setup, "a", id, role));
  assert(!strcmp(id, "D000001") && role == UserRole::Owner);
  assert(!RegistrationRecovery::reconcile(RegistrationRecovery::Kind::Enrollment, "a", id, role));
  assert(!RegistrationRecovery::reconcile(RegistrationRecovery::Kind::Setup, "b", id, role));
  assert(RegistrationRecovery::reconcile(RegistrationRecovery::Kind::Enrollment, "b", id, role));
  assert(!strcmp(id, "D000002") && role == UserRole::User);
  assert(!RegistrationRecovery::reconcile(RegistrationRecovery::Kind::Enrollment, "c", id, role));
  assert(!RegistrationRecovery::reconcile(RegistrationRecovery::Kind::Enrollment, "d", id, role));
  assert(!RegistrationRecovery::reconcile(RegistrationRecovery::Kind::Enrollment, "wrong", id, role));
  databaseHealthy = false;
  assert(!RegistrationRecovery::reconcile(RegistrationRecovery::Kind::Setup, "a", id, role));
  assert(id[0] == '\0');
  std::cout << "PASS: production RegistrationRecovery active identity, kind binding, revoked/orphan/wrong credential and unhealthy-store cases (9 assertions)\n";
}
