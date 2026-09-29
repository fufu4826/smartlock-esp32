#include "AdminDisplay.h"
#include "AdminBitmaps.h"
namespace {
void text(TFT_eSPI& t,const AdminLabel& l,int y){t.drawBitmap((320-l.w)/2,y,l.data,l.w,l.h,TFT_WHITE);}
void button(TFT_eSPI& t,const AdminLabel& l,int y){t.fillRoundRect(10,y,300,70,8,TFT_WHITE);t.drawBitmap((320-l.w)/2,y+(70-l.h)/2,l.data,l.w,l.h,TFT_BLACK);}
}
void renderAdmin(TFT_eSPI& t,const PhysicalAdmin& a){
 t.fillScreen(0x7800);t.setTextDatum(MC_DATUM);t.setTextColor(TFT_WHITE,0x7800);t.drawString("ADMIN",160,25,4);
 using P=PhysicalAdmin::Page;
 if(a.page()==P::Pin){text(t,a.result()==AdminPin::Result::Ok?adminLabel0:a.result()==AdminPin::Result::Locked?adminLabel14:a.result()==AdminPin::Result::StorageFailure?adminLabel15:adminLabel13,50);
  for(int i=0;i<4;++i){if(i<a.length())t.fillCircle(115+i*30,100,7,TFT_WHITE);else t.drawCircle(115+i*30,100,7,TFT_WHITE);}
  for(int row=0;row<4;++row)for(int col=0;col<3;++col){int x=10+col*100,y=130+row*66;t.fillRoundRect(x,y,96,60,6,TFT_WHITE);t.setTextColor(TFT_BLACK,TFT_WHITE);
   if(row<3)t.drawNumber(1+row*3+col,x+48,y+30,4);
   else if(col==1)t.drawNumber(0,x+48,y+30,4);
   else {const AdminLabel& l=col==0?adminLabel1:adminLabel2;t.drawBitmap(x+(96-l.w)/2,y+(60-l.h)/2,l.data,l.w,l.h,TFT_BLACK);}}
 }else if(a.page()==P::Menu){button(t,adminLabel4,130);button(t,adminLabel5,240);}
 else if(a.page()==P::Emergency){text(t,adminLabel7,140);button(t,adminLabel8,250);}
 else if(a.page()==P::Reset){text(t,adminLabel9,140);button(t,adminLabel10,250);}
 else if(a.page()==P::ResetFinal){text(t,adminLabel11,140);button(t,adminLabel12,250);}
 button(t,a.page()==P::Menu?adminLabel6:adminLabel3,410);
}
