#include "AdminPin.h"
#include <Preferences.h>
#include <esp_system.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <string.h>
namespace {
AdminPin::FailureCallback failureCallback = nullptr;
struct Record { uint8_t salt[16],hash[32]; };
void wipe(void* p,size_t n){volatile uint8_t* b=(volatile uint8_t*)p;while(n--)*b++=0;}
bool derive(const char* pin,Record& r,uint8_t out[32]){
 mbedtls_md_context_t c;mbedtls_md_init(&c);
 bool ok=mbedtls_md_setup(&c,mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),1)==0&&
 mbedtls_pkcs5_pbkdf2_hmac(&c,(const unsigned char*)pin,4,r.salt,16,10000,32,out)==0;
 mbedtls_md_free(&c);return ok;
}
}
bool AdminPin::present_=false,AdminPin::healthy_=false;
bool AdminPin::initialDefault_=false;
uint8_t AdminPin::failures_=0;
uint32_t AdminPin::lockedAt_=0,AdminPin::revision_=0;
void AdminPin::setFailureCallback(FailureCallback callback){failureCallback=callback;}
bool AdminPin::valid(const char* p){if(!p||strnlen(p,5)!=4)return false;for(int i=0;i<4;++i)if(p[i]<'0'||p[i]>'9')return false;return true;}
bool AdminPin::begin(bool configured, bool freshInstallation){
 Preferences n;healthy_=n.begin("sl-pin",false);if(!healthy_)return false;
 size_t len=n.getBytesLength("verifier");present_=len!=0;healthy_=!present_||len==sizeof(Record);
 failures_=n.getUChar("fail",0);initialDefault_=!configured&&n.getUChar("initial",0)==1;
 // Only a genuinely unconfigured installation may create the initial default.
 // A missing/corrupt verifier on a configured installation remains fail-closed.
 if(healthy_&&!present_&&!configured&&freshInstallation){
   if(n.putUChar("initial",1)!=1){n.end();healthy_=false;return false;}
   initialDefault_=true;n.end();lockedAt_=millis();return save("1234");
 }
 n.end();lockedAt_=millis();
 if(!healthy_)return false;
 if(!present_){healthy_=false;return false;}
 return true;
}
bool AdminPin::save(const char* p){
 Record r={};esp_fill_random(r.salt,16);bool ok=derive(p,r,r.hash);
 Preferences n;if(ok)ok=n.begin("sl-pin",false);
 if(ok){ok=n.putBytes("verifier",&r,sizeof(r))==sizeof(r);Record read={};ok=ok&&n.getBytes("verifier",&read,sizeof(read))==sizeof(read)&&!memcmp(&r,&read,sizeof(r));wipe(&read,sizeof(read));
 if(ok)ok=n.putUChar("fail",0)==1;n.end();}
 wipe(&r,sizeof(r));if(ok){present_=healthy_=true;failures_=0;++revision_;}else healthy_=false;return ok;
}
bool AdminPin::initialize(const char* p){
 if(!healthy_||(present_&&!initialDefault_)||!valid(p)||!save(p))return false;
 Preferences n;bool ok=n.begin("sl-pin",false);
 if(ok){ok=n.putUChar("initial",0)==1;n.end();}
 initialDefault_=false;if(!ok)healthy_=false;return ok;
}
AdminPin::Result AdminPin::verify(const char* p,uint32_t now){
 if(!healthy_||!present_)return Result::StorageFailure;
 if(failures_>=3&&uint32_t(now-lockedAt_)<60000)return Result::Locked;
 if(failures_>=3)failures_=0;
 if(!valid(p))return Result::Invalid;
 Record r={};uint8_t hash[32]={};Preferences n;
 if(!n.begin("sl-pin",false)||n.getBytes("verifier",&r,sizeof(r))!=sizeof(r)){n.end();wipe(&r,sizeof(r));healthy_=false;return Result::StorageFailure;}
 bool ok=derive(p,r,hash);uint8_t difference=0;for(int i=0;i<32;++i)difference|=hash[i]^r.hash[i];wipe(hash,32);wipe(&r,sizeof(r));
 if(!ok){n.end();return Result::StorageFailure;}
 failures_=difference?failures_+1:0;if(failures_>=3)lockedAt_=now;
 if(difference && failureCallback)failureCallback(failures_,failures_>=3);
 // One small durable write per completed attempt, never per countdown second.
 ok=n.putUChar("fail",failures_)==1;n.end();if(!ok){healthy_=false;return Result::StorageFailure;}
 return difference?(failures_>=3?Result::Locked:Result::Invalid):Result::Ok;
}
AdminPin::Result AdminPin::change(const char* old,const char* next,const char* confirm,uint32_t now){
 if(!valid(next)||!valid(confirm)||strcmp(next,confirm))return Result::Invalid;
 Result r=verify(old,now);if(r!=Result::Ok)return r;
 return save(next)?Result::Ok:Result::StorageFailure;
}
