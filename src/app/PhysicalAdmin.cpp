#include "PhysicalAdmin.h"
PhysicalAdmin::Action PhysicalAdmin::touch(uint16_t x,uint16_t y,bool hold,uint32_t now){
 const auto expiry=update(now);if(expiry!=Action::None)return expiry;
 if(!active())return Action::None;
 at_=now;dirty_=true;
 if(x<10||x>=310)return Action::None;
 if(y>=410){Serial.println("ADMIN: BACK");clear();return Action::Exit;}
 if(page_==Page::Pin){
  if(hold||x<10||x>=310||y<130||y>=394)return Action::None;
  int row=(y-130)/66,col=(x-10)/100;
  if(row<3){if(length_<4)pin_[length_++]=char('1'+row*3+col);}
  else if(col==0){if(length_)pin_[--length_]=0;}
  else if(col==1){if(length_<4)pin_[length_++]='0';}
  else if(length_==4){const uint32_t verificationStarted=millis();result_=AdminPin::verify(pin_,now);for(auto& c:pin_)c=0;length_=0;
   // Verification is synchronous. Start inactivity at completion, not before
   // the KDF/NVS work; never count verification time as menu inactivity.
   at_=now+uint32_t(millis()-verificationStarted);
   if(result_==AdminPin::Result::Ok){authorized_=true;page_=Page::Menu;Serial.println("ADMIN: PIN ACCEPTED -> MENU");}
   else Serial.println("ADMIN: PIN REJECTED -> PIN");}
  return Action::None;
 }
 if(!authorized_){clear();return Action::Exit;}
 if(page_==Page::Menu&&!hold){if(y>=130&&y<210)page_=Page::Emergency;else if(y>=240&&y<320)page_=Page::Reset;}
 else if(page_==Page::Emergency&&!hold&&y>=250&&y<330){clear();return Action::Unlock;}
 else if(page_==Page::Reset&&!hold&&y>=250&&y<330)page_=Page::ResetFinal;
 else if(page_==Page::ResetFinal&&hold&&y>=250&&y<330){clear();return Action::Reset;}
 return Action::None;
}
