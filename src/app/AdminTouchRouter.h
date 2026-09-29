#pragma once
#include "PhysicalAdmin.h"
#include "AppStateMachine.h"
#include "../hardware/TouchManager.h"

// Own a complete Admin gesture. Releases without a fresh press must never
// become buttons or fall through to the normal Management -> Access route.
class AdminTouchRouter {
 public:
 PhysicalAdmin::Action dispatch(PhysicalAdmin& admin,AppStateMachine& app,
     TouchEvent event,uint16_t x,uint16_t y,uint32_t now){
  using A=PhysicalAdmin::Action;
  const auto expiry=admin.update(now);
  if(expiry!=A::None){pressed_=false;app.begin(now,app.configured());return expiry;}
  if(admin.active()){
   if(event==TouchEvent::SingleTap){pressed_=true;return A::None;}
   if(event==TouchEvent::TapReleased||event==TouchEvent::Hold){
    if(!pressed_)return A::None;
    pressed_=false;
    const auto action=admin.touch(x,y,event==TouchEvent::Hold,now);
    if(action!=A::None){app.begin(millis(),app.configured());if(action!=A::Timeout)app.onSingleTap(millis());}
    return action;
   }
   return A::None;
  }
  if(event==TouchEvent::Hold&&app.state()==AppState::ManagementRequest){
   pressed_=false;admin.enter(now);Serial.println("ADMIN: MANAGEMENT HOLD -> PIN");
  }else if(event==TouchEvent::SingleTap)app.onSingleTap(now);
  else if(event==TouchEvent::TapReleased)app.onTapReleased(now);
  else if(event==TouchEvent::Hold)app.onHold(now);
  return A::None;
 }
 private:
 bool pressed_=false;
};
