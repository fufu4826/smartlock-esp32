#include "Diagnostics.h"
#include <Arduino.h>
#include <memory>
#include <cstring>
#include "../storage/IdentityStore.h"
#include "../storage/AuthStore.h"
namespace { void (*systemDiagnostic)() = nullptr; }
void Diagnostics::setSystemDiagnostic(void (*callback)()) { systemDiagnostic = callback; }
void Diagnostics::poll() {
  // USB-only, fixed read-only diagnostic. No auth record/credential output,
  // arbitrary filename, reset, configuration write or unlock command exists.
  static char command[24] = {};
  static uint8_t used = 0;
  static bool overflow = false;
  for (uint8_t budget = 0; budget < 32 && Serial.available(); ++budget) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c != '\n') {
      if (used < sizeof(command) - 1) command[used++] = c;
      else overflow = true;
      continue;
    }
    command[used] = '\0';
    const bool authRequested = !overflow && !strcmp(command, "DIAG_AUTH");
    const bool systemRequested = !overflow && !strcmp(command, "DIAG_SYSTEM");
    const bool commandValid=!overflow;
    used = 0; overflow = false;
    if (authRequested) {
      using namespace smartlock::storage;
      std::unique_ptr<IdentityRecord[]> rows(new(std::nothrow) IdentityRecord[IdentityStore::kMaxRecords]);
      size_t count=0,auth=0,owners=0;
      const bool ok=rows && IdentityStore::load(rows.get(),IdentityStore::kMaxRecords,count) &&
          AuthStore::inspect(auth)==AuthStore::ReadResult::Valid;
      if(ok) for(size_t i=0;i<count;++i) {
        if(rows[i].role==UserRole::Owner)++owners;
        Serial.printf("IDENTITY META: id=%s name=%s role=%u status=%u verifier=%u\n",rows[i].id,
            rows[i].name,(unsigned)rows[i].role,(unsigned)rows[i].status,AuthStore::hasDevice(rows[i].id));
      }
      Serial.printf("AUTH INSPECT: valid=%u identities=%u owners=%u verifiers=%u OWNER_VERIFIER_PRESERVED=%u heap=%u min_heap=%u\n",
          ok,(unsigned)count,(unsigned)owners,(unsigned)auth,IdentityStore::backupVerifierUnchanged(),
          ESP.getFreeHeap(),ESP.getMinFreeHeap());
      Serial.println("AUTH INSPECT END");
      continue;
    }
    if (systemRequested) {
      if (systemDiagnostic) systemDiagnostic();
      Serial.println("SYSTEM DIAG END");
      continue;
    }
  }
}
