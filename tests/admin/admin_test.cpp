#include "../../src/app/PhysicalAdmin.h"
#include "../../src/hardware/LockController.h"
#include "../../src/storage/ConfigStore.h"
#include <map>
#include <string>
#include <cassert>
#include <cstdio>
uint32_t clockMs=0;bool failed=false;std::map<std::string,std::string> records;
uint32_t verificationDelay=0;
void transitionChecks();
int lockLevel=1;
using R=AdminPin::Result;using A=PhysicalAdmin::Action;using P=PhysicalAdmin::Page;
int checks=0;
int pinEvents=0; unsigned lastAttempt=0; bool lastLockout=false;
#define CHECK(x) do{++checks;assert(x);}while(0)
void digits(PhysicalAdmin& p,const char* s,uint32_t now){for(;*s;++s){int v=*s-'0';int row=v?(v-1)/3:3,col=v?(v-1)%3:1;p.touch(20+col*100,140+row*66,false,now);}}
int main(){
 AdminPin::setFailureCallback([](uint8_t count,bool lockout){++pinEvents;lastAttempt=count;lastLockout=lockout;});
 CHECK(!AdminPin::begin(true));CHECK(records["verifier"].empty());
 CHECK(!AdminPin::begin(false));CHECK(records["verifier"].empty());
 CHECK(AdminPin::begin(false,true));CHECK(records["verifier"].size()==48);CHECK(AdminPin::verify("1234",0)==R::Ok);CHECK(AdminPin::initialize("1234"));CHECK(records["verifier"].size()==48);CHECK(records["verifier"].find("1234")==std::string::npos);
 CHECK(!AdminPin::valid("123"));CHECK(!AdminPin::valid("12345"));CHECK(!AdminPin::valid("12x4"));CHECK(AdminPin::valid("0000"));
 CHECK(AdminPin::verify("1234",0)==R::Ok);CHECK(AdminPin::verify("9999",0)==R::Invalid);CHECK(AdminPin::verify("9999",0)==R::Invalid);CHECK(AdminPin::verify("9999",0)==R::Locked);CHECK(AdminPin::verify("1234",59999)==R::Locked);
 CHECK(pinEvents==3&&lastAttempt==3&&lastLockout);
 CHECK(AdminPin::verify("bad",59999)==R::Locked);CHECK(pinEvents==3);
 clockMs=100;CHECK(AdminPin::begin(true));CHECK(AdminPin::verify("1234",60099)==R::Locked);CHECK(AdminPin::verify("1234",60100)==R::Ok);CHECK(records["fail"][0]==0);
 CHECK(AdminPin::change("1234","5678","5679",60100)==R::Invalid);CHECK(AdminPin::change("1234","5678","5678",60100)==R::Ok);
 auto saved=records["verifier"];CHECK(AdminPin::begin(true));CHECK(records["verifier"]==saved);CHECK(AdminPin::verify("1234",60101)==R::Invalid);CHECK(AdminPin::verify("5678",60102)==R::Ok);
 PhysicalAdmin p;p.enter(70000);CHECK(p.page()==P::Pin);digits(p,"567",70001);CHECK(p.length()==3);CHECK(p.touch(220,340,false,70002)==A::None);CHECK(p.page()==P::Pin);
 p.touch(20,340,false,70003);CHECK(p.length()==2);digits(p,"78",70004);CHECK(p.length()==4);p.touch(220,340,false,70005);CHECK(p.page()==P::Menu);
 CHECK(p.touch(20,140,false,70006)==A::None);CHECK(p.page()==P::Emergency);CHECK(p.touch(20,260,false,70007)==A::Unlock);CHECK(!p.active());CHECK(p.touch(20,260,false,70008)==A::None);
 p.enter(80000);digits(p,"5678",80001);p.touch(220,340,false,80002);p.touch(20,250,false,80003);CHECK(p.page()==P::Reset);CHECK(p.touch(20,260,false,80004)==A::None);CHECK(p.page()==P::ResetFinal);CHECK(p.touch(20,260,false,80005)==A::None);CHECK(p.touch(20,260,true,80006)==A::Reset);CHECK(!p.active());
 p.enter(90000);digits(p,"5678",90001);p.touch(220,340,false,90002);CHECK(p.update(120002)==A::Timeout);CHECK(!p.active());CHECK(p.length()==0);CHECK(p.touch(20,260,false,120003)==A::None);
 p.enter(130000);digits(p,"56",130001);CHECK(p.touch(20,420,false,130002)==A::Exit);CHECK(p.length()==0);
 p.enter(140000);digits(p,"5678",140001);p.touch(220,340,false,140002);CHECK(AdminPin::change("5678","0123","0123",140003)==R::Ok);CHECK(p.update(140004)==A::Timeout);
 failed=true;CHECK(AdminPin::verify("0123",150000)==R::StorageFailure);CHECK(!AdminPin::begin(true));
 LockController lock;lock.begin();CHECK(lock.isLocked());CHECK(lockLevel==1);clockMs=200000;
 CHECK(lock.unlock(5000)==LockController::UnlockResult::Unlocked);CHECK(lockLevel==0);lock.update(204999);CHECK(!lock.isLocked());lock.update(205000);CHECK(lock.isLocked());CHECK(lockLevel==1);
 CHECK(lock.unlock(0)==LockController::UnlockResult::InvalidDuration);CHECK(lockLevel==1);
 failed=false;records.clear();CHECK(AdminPin::begin(false,true));CHECK(records["verifier"].size()==48);CHECK(AdminPin::verify("1234",210000)==R::Ok);CHECK(AdminPin::begin(false,true));CHECK(AdminPin::initialize("4321"));CHECK(!AdminPin::initialize("1234"));CHECK(AdminPin::begin(true));CHECK(AdminPin::verify("4321",210000)==R::Ok);CHECK(AdminPin::begin(false));CHECK(!AdminPin::initialize("1234"));CHECK(AdminPin::verify("4321",210001)==R::Ok);
 saved=records["verifier"];CHECK(AdminPin::begin(false));CHECK(records["verifier"]==saved);
 records.erase("verifier");CHECK(!AdminPin::begin(false));CHECK(records["verifier"].empty());
 records.clear();ConfigStore config;CHECK(config.begin()==ConfigStore::Result::CreatedDefaults);auto next=config.config();next.unlockDurationMs=7000;CHECK(config.save(next));ConfigStore reboot;CHECK(reboot.begin()==ConfigStore::Result::Ok);CHECK(reboot.config().unlockDurationMs==7000);
 auto savedConfig=records;records["active"]="unsupported-retired-selector";ConfigStore retired;CHECK(retired.begin()==ConfigStore::Result::Corrupt);CHECK(!retired.healthy());records=savedConfig;records["txn"]="unsupported-retired-journal";ConfigStore journal;CHECK(journal.begin()==ConfigStore::Result::Corrupt);
 records=savedConfig;records["cfg-b"][0]^=1;ConfigStore corrupt;CHECK(corrupt.begin()==ConfigStore::Result::Corrupt);CHECK(!corrupt.healthy());
 // Management unlock duration: single ConfigStore field, 1..60 s bounds enforced by the store, persists across reload.
 records.clear();{ConfigStore c;CHECK(c.begin()==ConfigStore::Result::CreatedDefaults);auto v=c.config();
  v.unlockDurationMs=999;CHECK(!c.save(v));v.unlockDurationMs=60001;CHECK(!c.save(v));CHECK(c.config().unlockDurationMs==10000);
  v.unlockDurationMs=1000;CHECK(c.save(v));v.unlockDurationMs=60000;CHECK(c.save(v));
  ConfigStore again;CHECK(again.begin()==ConfigStore::Result::Ok);CHECK(again.config().unlockDurationMs==60000);}
 // Management Admin PIN change: wrong current PIN shares the verify counter/lockout; new PIN survives reload; old PIN rejected.
 records.clear();clockMs=300000;pinEvents=0;CHECK(AdminPin::begin(false,true));CHECK(AdminPin::initialize("2468"));
 CHECK(AdminPin::change("1111","1357","1357",300001)==R::Invalid);CHECK(AdminPin::change("1112","1357","1357",300002)==R::Invalid);
 CHECK(AdminPin::change("1113","1357","1357",300003)==R::Locked);CHECK(pinEvents==3&&lastLockout);
 CHECK(AdminPin::change("2468","1357","1357",300004)==R::Locked);CHECK(AdminPin::verify("2468",359999)==R::Locked);
 CHECK(AdminPin::change("2468","13a7","13a7",360004)==R::Invalid);CHECK(pinEvents==3);
 CHECK(AdminPin::change("2468","1357","1357",360005)==R::Ok);CHECK(records["verifier"].find("1357")==std::string::npos);
 CHECK(AdminPin::begin(true));CHECK(AdminPin::verify("2468",360006)==R::Invalid);CHECK(AdminPin::verify("1357",360007)==R::Ok);
 printf("Admin focused checks: %d PASS (isolated NVS/crypto fakes)\n",checks);
 transitionChecks();
}
