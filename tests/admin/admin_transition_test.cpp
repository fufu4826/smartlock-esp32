#include "../../src/app/AdminTouchRouter.h"
#include "../../src/app/LocalFallbackTouch.h"
#include "../../src/hardware/LockController.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
extern uint32_t clockMs,verificationDelay;
extern int lockLevel;
extern int pinEvents;
extern std::map<std::string,std::string> records;
using A=PhysicalAdmin::Action;using P=PhysicalAdmin::Page;using E=TouchEvent;
void transitionChecks(){
 {
  LocalFallbackTouch fallback;using F=LocalFallbackTouch::Action;
  assert(fallback.update(AppState::IdleScreenOff,E::SingleTap,440,0)==F::None);
  assert(fallback.update(AppState::ManagementRequest,E::TapReleased,440,0)==F::None);
  assert(fallback.update(AppState::AccessRequest,E::SingleTap,440,1)==F::Consume);
  assert(fallback.update(AppState::AccessRequest,E::TapReleased,440,2)==F::ShowWifi);
  assert(fallback.active());
  assert(fallback.update(AppState::AccessRequest,E::TapReleased,0,3)==F::Consume);
  assert(fallback.update(AppState::AccessRequest,E::SingleTap,0,4)==F::Consume);
  assert(fallback.update(AppState::AccessRequest,E::Hold,0,5)==F::Consume);
  assert(fallback.update(AppState::AccessRequest,E::TapReleased,0,6)==F::Consume);
  assert(fallback.update(AppState::AccessRequest,E::SingleTap,0,7)==F::Consume);
  assert(fallback.update(AppState::AccessRequest,E::TapReleased,0,8)==F::ReturnToQr);
  assert(!fallback.active());
  assert(fallback.update(AppState::ManagementRequest,E::SingleTap,440,0xfffffff0U)==F::Consume);
  assert(fallback.update(AppState::ManagementRequest,E::TapReleased,440,0xfffffff1U)==F::ShowWifi);
  assert(fallback.update(AppState::ManagementRequest,E::None,0,119984U)==F::Consume);
  assert(fallback.update(AppState::ManagementRequest,E::None,0,119985U)==F::Expired);
  AppStateMachine refresh;refresh.begin(0,true);refresh.onSingleTap(0);
  refresh.refreshRequest(29000);refresh.update(30001);assert(refresh.state()==AppState::AccessRequest);
  refresh.update(59000);assert(refresh.state()==AppState::IdleScreenOff);
 }
 records.clear();clockMs=300000;verificationDelay=0;
 assert(AdminPin::begin(false,true)&&AdminPin::initialize("4321"));
 LockController lock;lock.begin();
 PhysicalAdmin admin;AppStateMachine app;AdminTouchRouter router;
 app.begin(clockMs,true);
 auto event=[&](E e,int x=0,int y=0){return router.dispatch(admin,app,e,uint16_t(x),uint16_t(y),clockMs);};
 auto tap=[&](int x,int y){event(E::SingleTap,x,y);++clockMs;return event(E::TapReleased,x,y);};
 auto pin=[&](const char* value){for(;*value;++value){int v=*value-'0';tap(20+(v?(v-1)%3:1)*100,140+(v?(v-1)/3:3)*66);}return tap(220,340);};
 event(E::SingleTap);assert(app.state()==AppState::AccessRequest);
 event(E::Hold);assert(app.state()==AppState::ManagementRequest);
 event(E::SingleTap);event(E::Hold);assert(admin.page()==P::Pin);
 event(E::TapReleased,20,420);assert(admin.page()==P::Pin); // stale entry release, even on Back
 pin("9999");assert(admin.page()==P::Pin&&!admin.authorized()&&lock.isLocked());
 assert(app.state()==AppState::ManagementRequest);
 // Reproduce synchronous verification lasting longer than the inactivity budget.
 verificationDelay=35000;pin("4321");verificationDelay=0;
 assert(admin.page()==P::Menu&&admin.authorized()&&lock.isLocked());
 event(E::None);assert(admin.page()==P::Menu); // fresh loop time after blocking KDF
 event(E::TapReleased,20,420);assert(admin.page()==P::Menu); // stale release cannot Back/Access
 event(E::TapReleased,20,140);assert(admin.page()==P::Menu); // cannot select Emergency
 event(E::None);assert(admin.page()==P::Menu&&lockLevel==1);
 tap(20,140);assert(admin.page()==P::Emergency&&lock.isLocked());
 event(E::TapReleased,20,260);assert(admin.page()==P::Emergency&&lock.isLocked());
 assert(tap(20,260)==A::Unlock&&!admin.active()); // explicit fresh confirmation only
 assert(lockLevel==1); // routing only emits action; it never writes GPIO or unlocks itself
 event(E::Hold);event(E::SingleTap);event(E::Hold);pin("4321");
 assert(admin.page()==P::Menu);assert(tap(20,420)==A::Exit);
 assert(!admin.authorized()&&app.state()==AppState::AccessRequest);
 event(E::Hold);event(E::SingleTap);event(E::Hold);pin("4321");
 clockMs+=29999;event(E::None);assert(admin.page()==P::Menu);
 ++clockMs;assert(event(E::None)==A::Timeout);
 assert(!admin.active()&&!admin.authorized()&&app.state()==AppState::IdleScreenOff&&lock.isLocked());
 event(E::TapReleased);assert(app.state()==AppState::IdleScreenOff);
 event(E::SingleTap);assert(app.state()==AppState::AccessRequest);
 // Actual touch debounce, hold suppression, and short releases across routing.
 TFT_eSPI tft;TouchManager touch;touch.begin(tft);admin.clear();app.begin(clockMs,true);
 auto sample=[&](bool down,uint32_t delta){clockMs+=delta;tft.down=down;auto e=touch.update(clockMs);event(e,touch.x(),touch.y());return e;};
 sample(true,1);assert(sample(true,65)==E::SingleTap);assert(app.state()==AppState::AccessRequest);
 assert(sample(true,1500)==E::Hold);assert(app.state()==AppState::ManagementRequest);
 sample(false,1);assert(sample(false,65)==E::None);assert(app.state()==AppState::ManagementRequest);
 sample(true,1);sample(true,65);assert(sample(true,1500)==E::Hold);assert(admin.page()==P::Pin);
 sample(false,1);assert(sample(false,65)==E::None);assert(admin.page()==P::Pin);
 admin.clear();app.begin(clockMs,true);sample(true,1);sample(true,65);sample(false,1);
 assert(sample(false,65)==E::TapReleased&&app.state()==AppState::AccessRequest);
 // The former physical LINE setup coordinates are inert. Keep the normal
 // Admin menu authorization and locked state unchanged, then verify the
 // remaining physical emergency and reset routes still work.
 app.begin(clockMs,true);event(E::Hold);event(E::SingleTap);event(E::Hold);
 pin("4321");assert(admin.page()==P::Menu&&admin.authorized());
 const int failuresBeforeRemovedTarget=pinEvents;
 assert(tap(160,360)==A::None&&admin.page()==P::Menu&&admin.authorized());
 assert(tap(160,395)==A::None&&admin.page()==P::Menu&&admin.authorized());
 assert(app.state()==AppState::ManagementRequest&&pinEvents==failuresBeforeRemovedTarget);
 assert(lock.isLocked()&&lockLevel==1);
 assert(tap(160,160)==A::None&&admin.page()==P::Emergency&&admin.authorized());
 assert(tap(160,420)==A::Exit&&admin.page()==P::Off&&!admin.authorized());
 assert(app.state()==AppState::AccessRequest);
 // Reset retains its two-step confirmation and hold requirement.
 app.begin(clockMs,true);event(E::Hold);event(E::SingleTap);event(E::Hold);
 pin("4321");assert(tap(160,280)==A::None&&admin.page()==P::Reset);
 assert(tap(160,280)==A::None&&admin.page()==P::ResetFinal);
 assert(event(E::Hold,160,280)==A::None);
 event(E::SingleTap,160,280);assert(event(E::Hold,160,280)==A::Reset);
 assert(!admin.active()&&lock.isLocked()&&lockLevel==1);
 // Changing the PIN/auth revision still invalidates an authorized menu.
 app.begin(clockMs,true);event(E::Hold);event(E::SingleTap);event(E::Hold);
 pin("4321");
 assert(AdminPin::change("4321","4321","4321",clockMs)==AdminPin::Result::Ok);
 assert(event(E::None)==A::Timeout&&admin.page()==P::Off&&!admin.authorized());
 assert(pinEvents==failuresBeforeRemovedTarget&&lock.isLocked()&&lockLevel==1);
 printf("Admin production router/gesture regression: all 10 requested cases PASS (isolated clock/crypto/GPIO)\n");
}
