#include "TouchManager.h"
#include "AppStateMachine.h"
#include <cassert>
#include <iostream>
int main(){
  TFT_eSPI screen; TouchManager touch; AppStateMachine app;
  touch.begin(screen);app.begin(0,true);
  unsigned holds=0;uint32_t now=0;
  auto step=[&](bool down,uint32_t delta){now+=delta;screen.down=down;
    auto e=touch.update(now);
    if(e==TouchEvent::SingleTap)app.onSingleTap(now);
    if(e==TouchEvent::TapReleased)app.onTapReleased(now);
    if(e==TouchEvent::Hold){++holds;app.onHold(now);}
    return e;
  };
  step(true,1);step(true,65);assert(app.state()==AppState::AccessRequest);
  step(true,1500);assert(app.state()==AppState::ManagementRequest&&holds==1);
  step(true,2000);assert(app.state()==AppState::ManagementRequest&&holds==1);
  step(false,1);step(false,65);assert(app.state()==AppState::ManagementRequest);
  step(true,1);step(true,65);assert(app.state()==AppState::ManagementRequest);
  step(true,1500);assert(app.state()==AppState::ResetRequest&&holds==2);
  step(true,2000);assert(app.state()==AppState::ResetRequest&&holds==2);
  step(false,1);step(false,65);assert(app.state()==AppState::ResetRequest);
  app.resetChoice(false,now);assert(app.state()==AppState::AccessRequest);
  step(true,1);step(true,65);step(true,1500);
  step(false,1);step(false,65);assert(app.state()==AppState::ManagementRequest);
  // Short press returns only on release, preserving immediate wake elsewhere.
  step(true,1);step(true,65);assert(app.state()==AppState::ManagementRequest);
  step(false,100);step(false,65);assert(app.state()==AppState::AccessRequest);
  app.onHold(now);app.onHold(now);assert(app.state()==AppState::ResetRequest);
  app.resetChoice(true,now);assert(app.state()==AppState::ResetConfirm);
  app.onSingleTap(now);app.onTapReleased(now);app.onHold(now);
  assert(app.state()==AppState::ResetConfirm); // still needs explicit OK
  app.begin(now,false);app.onSingleTap(now);app.onHold(now);assert(app.state()==AppState::SetupRequest);
  std::cout<<"PASS: production touch/state second hold reaches Reset; one Hold per contact; short-release Access; Yes still requires OK; Setup unchanged\n";
}
