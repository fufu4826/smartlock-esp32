#pragma once
#include "AppStateMachine.h"
#include "../hardware/TouchManager.h"

// Own the whole fallback gesture so it cannot become an Admin hold or Access tap.
class LocalFallbackTouch {
 public:
 enum class Action { None, Consume, ShowWifi, ReturnToQr, Expired };
 bool active() const { return active_; }
 void clear(){active_=pressed_=button_=false;}
 Action update(AppState state,TouchEvent event,uint16_t y,uint32_t now){
  if(active_&&static_cast<uint32_t>(now-started_)>=120000){clear();return Action::Expired;}
  if(active_){
   if(event==TouchEvent::SingleTap)pressed_=true;
   if(event==TouchEvent::TapReleased&&pressed_){clear();return Action::ReturnToQr;}
   if(event==TouchEvent::Hold)pressed_=false;
   return Action::Consume;
  }
  if(button_){
   if(event==TouchEvent::Hold){button_=false;pressed_=false;return Action::Consume;}
   if(event==TouchEvent::TapReleased){button_=false;active_=true;started_=now;return Action::ShowWifi;}
   return Action::Consume;
  }
  if(event==TouchEvent::SingleTap&&y>=412&&y<470&&
     (state==AppState::AccessRequest||state==AppState::ManagementRequest)){
   button_=true;return Action::Consume;
  }
  return Action::None;
 }
 private:
 bool active_=false,pressed_=false,button_=false;
 uint32_t started_=0;
};
