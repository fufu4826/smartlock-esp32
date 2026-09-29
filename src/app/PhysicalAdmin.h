#pragma once
#include <stdint.h>
#include "../security/AdminPin.h"
class PhysicalAdmin {
 public:
 enum class Page { Off,Pin,Menu,Emergency,Reset,ResetFinal };
 enum class Action { None,Exit,Unlock,Reset,Timeout };
 void enter(uint32_t now){clear();result_=AdminPin::Result::Ok;page_=Page::Pin;at_=now;revision_=AdminPin::revision();dirty_=true;}
 void clear(){volatile char* p=pin_;for(int i=0;i<5;++i)p[i]=0;length_=0;authorized_=false;page_=Page::Off;dirty_=true;}
 bool active()const{return page_!=Page::Off;}
 bool authorized()const{return authorized_;}
 Page page()const{return page_;}
 uint8_t length()const{return length_;}
 AdminPin::Result result()const{return result_;}
 bool dirty(){bool d=dirty_;dirty_=false;return d;}
 Action update(uint32_t now){
  const bool expired=static_cast<int32_t>(now-at_)>=30000;
  if(active()&&(expired||revision_!=AdminPin::revision())){Serial.println(revision_!=AdminPin::revision()?"ADMIN: AUTH REVISION EXPIRED":"ADMIN: INACTIVITY EXPIRED");clear();return Action::Timeout;}
  return Action::None;
 }
 Action touch(uint16_t x,uint16_t y,bool hold,uint32_t now);
 private:
 Page page_=Page::Off;char pin_[5]={};uint8_t length_=0;
 bool authorized_=false,dirty_=false;uint32_t at_=0,revision_=0;
 AdminPin::Result result_=AdminPin::Result::Ok;
};
