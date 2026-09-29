#include "identity_test_support.h"
#include <map>
#include "esp_wifi.h"
#include "bootloader_random.h"
#include <vector>

SDClass SD;
SerialClass Serial;
namespace {
uint32_t fakeNow=0,randomState=0x6d2b79f5u;
struct AuthEntry {std::string credential;};
std::map<std::string,AuthEntry> auth;
std::string verifier="simulated-owner-verifier-unchanged";
std::string auditUser,auditDevice,auditClaimed;
smartlock::events::Source auditSource=smartlock::events::Source::Unknown;
std::string credentialKey(const char* value){
  std::string key=value?value:"";
  if(key.size()==64)for(char& c:key)if(c>='A'&&c<='F')c=static_cast<char>(c-'A'+'a');
  return key;
}
}
uint32_t millis(){return fakeNow;}
void setFakeMillis(uint32_t value){fakeNow=value;}
void bootloader_random_enable(){}
void bootloader_random_disable(){}
esp_err_t esp_wifi_get_mode(wifi_mode_t* mode){if(mode)*mode=WIFI_MODE_NULL;return ESP_OK;}
void esp_fill_random(void* output,size_t length){auto* bytes=static_cast<uint8_t*>(output);for(size_t i=0;i<length;++i){randomState=1664525u*randomState+1013904223u;bytes[i]=static_cast<uint8_t>(randomState>>24);}}
namespace CanonicalOrigin {const char* host(){return "smartlock-0123456789ab.local";}}
namespace Audit {
void record(smartlock::events::Action,smartlock::events::Result,const char* user,const char* device){auditUser=user?user:"";auditDevice=device?device:"";}
void granted(const char* user,const char* device,smartlock::events::Source source){auditUser=user?user:"";auditDevice=device?device:"";auditSource=source;auditClaimed=device?device:"";}
void denied(const char* claimed,const char* verified,smartlock::events::Source source,smartlock::events::Result){auditDevice=verified?verified:"";auditClaimed=claimed?claimed:"";auditSource=source;}
const char* lastUser(){return auditUser.c_str();}const char* lastDevice(){return auditDevice.c_str();}
const char* lastClaimed(){return auditClaimed.c_str();}smartlock::events::Source lastSource(){return auditSource;}
}
void resetIdentityFakes(){SD.clear();auth.clear();fakeNow=0;randomState=0x6d2b79f5u;auditUser.clear();auditDevice.clear();auditClaimed.clear();auditSource=smartlock::events::Source::Unknown;}
void seedAuth(const char* id,const char* credential){auth[id]={credentialKey(credential)};}
const std::string& verifierImage(){return verifier;}
AuthStore::ReadResult AuthStore::inspect(size_t& count){count=auth.size();return auth.empty()?ReadResult::Missing:ReadResult::Valid;}
bool AuthStore::hasDevice(const char* id){return id&&auth.count(id)>0;}
bool AuthStore::verifyDevice(const char* id,const char* credential){auto it=id?auth.find(id):auth.end();return it!=auth.end()&&credential&&it->second.credential==credentialKey(credential);}
bool AuthStore::addDevice(const char* id,const char* credential){if(!id||!credential||auth.count(id))return false;const std::string key=credentialKey(credential);for(const auto& entry:auth)if(entry.second.credential==key)return false;auth[id]={key};return true;}
bool AuthStore::ownerVerifierMatches(const char* path){return path&&SD.fileContents(path)==verifier&&SD.fileContents("/smartlock/db/auth.rec")==verifier;}
bool AuthStore::saveFirst(const char* id,const char* credential){if(!id||!credential||auth.count(id))return false;auth[id]={credential};return true;}
bool AuthStore::verifyFirst(const char* id,const char* credential){return verifyDevice(id,credential);}

namespace LineNotifications { unsigned emitted=0; void unlockSuccess(const char* id,const char*,const char*,const char*){++emitted;auditDevice=id;} }
