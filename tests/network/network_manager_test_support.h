#pragma once

#include <string>

#include "WiFi.h"

struct FakeSecretsState {
  std::string savedSsid;
  std::string savedPassword;
  unsigned saveCalls = 0;
  unsigned loadCalls = 0;
  bool loadValid = false;
  bool saveShouldFail = false;
};

extern FakeSecretsState fakeSecrets;
void resetFakes();
