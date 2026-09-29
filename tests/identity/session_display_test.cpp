#include "session_display_test_support.h"
#include <cassert>
#include <cstring>
#include <iostream>

SessionManager sessions;
FakeApp app;
FakeDisplay displayManager;
FakeQr qr;
TFT_eSPI tft;
char sessionToken[65]={};
char qrPayload[256]={};

int main(){
  setFakeMillis(20100); // simulated HTTP/body work elapsed after loop timestamp 100
  app.current=AppState::AccessRequest;
  renderCurrentState(100);
  const char* path=std::strstr(qr.payload,"/a/");assert(path);
  const char* token=path+3;assert(std::strlen(token)==64);
  assert(sessions.validateSession(SessionType::Access,token,20100));
  assert(sessions.validateSession(SessionType::Access,token,20100+29999));
  assert(!sessions.validateSession(SessionType::Access,token,20100+30000));
  std::cout<<"PASS: production renderCurrentState creates Access session from fresh millis after stale loop timestamp\n";
}
