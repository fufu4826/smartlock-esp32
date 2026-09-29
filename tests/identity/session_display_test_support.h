#pragma once
#include "identity_test_support.h"
#include "AppState.h"
#include "SessionManager.h"

#include <cstdint>
#include <cstdio>

constexpr uint16_t TFT_BLUE=1,TFT_WHITE=2,TFT_YELLOW=3,TFT_RED=4;
struct TFT_eSPI {};
struct FakeApp { AppState current=AppState::AccessRequest; AppState state()const{return current;} };
struct FakeDisplay { AppState last=AppState::ErrorStatus;void render(AppState s){last=s;}void backlightOn(){} };
struct FakeQr {
  char payload[256]={};
  bool render(TFT_eSPI&,const char* value,const char*,uint16_t){std::snprintf(payload,sizeof(payload),"%s",value);return true;}
};

extern SessionManager sessions;
extern FakeApp app;
extern FakeDisplay displayManager;
extern FakeQr qr;
extern TFT_eSPI tft;
extern char sessionToken[65];
extern char qrPayload[256];
void renderCurrentState(uint32_t staleLoopTimestamp);
