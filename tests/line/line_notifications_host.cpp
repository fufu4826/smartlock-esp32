#include <assert.h>
#include <algorithm>
#include <stdio.h>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <time.h>

#define strcasecmp _stricmp
inline struct tm* gmtime_r(const time_t* timeValue, struct tm* output) {
  return gmtime_s(output, timeValue) == 0 ? output : nullptr;
}

#define LINE_NOTIFICATIONS_HOST_TEST 1
#include "../../src/notifications/LineNotifications.cpp"
using namespace LineNotifications;

namespace {
uint32_t nowMs = 100;
uint32_t randomWord = 0x12345678;
bool timeValid = true;
uint32_t epochValue = 1790000000;
std::map<std::string,std::vector<uint8_t>> nvs;
std::string failPutKey;
std::string corruptReadKey;
FakeSocketState socketState;
std::string socketResponse;
}
FakeESP ESP;

uint32_t fakeMillis() { return nowMs; }
void fakeAdvance(uint32_t ms) { nowMs += ms; }
uint32_t esp_random() { randomWord = randomWord * 1664525u + 1013904223u; return randomWord; }

bool Preferences::begin(const char* ns,bool readOnly){ns_=ns;ro_=readOnly;if(!readOnly)return true;const std::string prefix=std::string(ns)+":";for(const auto& item:nvs)if(item.first.rfind(prefix,0)==0)return true;return false;}
void Preferences::end(){ns_=nullptr;}
size_t Preferences::getBytesLength(const char* key){auto it=nvs.find(std::string(ns_)+":"+key);return it==nvs.end()?0:it->second.size();}
size_t Preferences::getBytes(const char* key,void* dst,size_t len){const std::string full=std::string(ns_)+":"+key;auto it=nvs.find(full);if(it==nvs.end()||it->second.size()!=len)return 0;memcpy(dst,it->second.data(),len);if(full==corruptReadKey){corruptReadKey.clear();static_cast<uint8_t*>(dst)[0]^=1;}return len;}
size_t Preferences::putBytes(const char* key,const void* src,size_t len){if(ro_)return 0;const std::string full=std::string(ns_)+":"+key;if(full==failPutKey)return 0;auto& v=nvs[full];v.assign(static_cast<const uint8_t*>(src),static_cast<const uint8_t*>(src)+len);return len;}
uint8_t Preferences::getUChar(const char* key,uint8_t fallback){auto it=nvs.find(std::string(ns_)+":"+key);return it==nvs.end()||it->second.size()!=1?fallback:it->second[0];}
size_t Preferences::putUChar(const char* key,uint8_t value){if(ro_)return 0;nvs[std::string(ns_)+":"+key]={value};return 1;}
bool Preferences::remove(const char* key){if(ro_)return false;nvs.erase(std::string(ns_)+":"+key);return true;}

SemaphoreHandle_t xSemaphoreCreateMutex(){return new int(0);}
SemaphoreHandle_t xSemaphoreCreateBinary(){return new int(0);}
int xSemaphoreTake(SemaphoreHandle_t s,uint32_t){if(s)*static_cast<int*>(s)=0;return 1;}
int xSemaphoreGive(SemaphoreHandle_t s){if(s)*static_cast<int*>(s)=1;return 1;}
int xTaskCreatePinnedToCore(TaskFunction_t,const char*,uint32_t,void*,uint32_t,TaskHandle_t* out,int){if(out)*out=reinterpret_cast<void*>(1);return 1;}
void vTaskDelay(uint32_t ms){fakeAdvance(ms);}

FakeSocketState& fakeSocket(){return socketState;}
void WiFiClientSecure::setCACert(const char* cert){assert(cert&&strstr(cert,"BEGIN CERTIFICATE"));}
void WiFiClientSecure::setHandshakeTimeout(unsigned long seconds){assert(seconds>0&&seconds<=10);}
int WiFiClientSecure::setTimeout(uint32_t seconds){assert(seconds<=2);return 1;}
int WiFiClientSecure::connect(const char* host,uint16_t port,int32_t timeout){
  assert(std::string(host)=="api.line.me");assert(port==443);assert(timeout>0&&timeout<=9000);
  at_=0;socketState.request.clear();socketState.response=socketResponse;connected_=socketState.connectOk;return connected_?1:0;
}
bool WiFiClientSecure::connected(){return connected_&&at_<socketState.response.size();}
size_t WiFiClientSecure::write(const uint8_t* data,size_t len){socketState.request.append(reinterpret_cast<const char*>(data),len);return len;}
int WiFiClientSecure::available(){return static_cast<int>(socketState.response.size()-at_);}
int WiFiClientSecure::read(){return at_<socketState.response.size()?static_cast<unsigned char>(socketState.response[at_++]):-1;}
int WiFiClientSecure::read(uint8_t* dst,size_t len){size_t n=std::min(len,socketState.response.size()-at_);memcpy(dst,socketState.response.data()+at_,n);at_+=n;return static_cast<int>(n);}
void WiFiClientSecure::stop(){connected_=false;}

uint32_t TimeManager::epochSeconds(){return epochValue;}
bool TimeManager::synchronized(){return timeValid;}
void TimeManager::update(bool,uint32_t){}

static void setResponse(int code,const char* extra,const std::string& body){
  char status[64];snprintf(status,sizeof(status),"HTTP/1.1 %d Mock\r\n",code);
  socketResponse=status;
  if(extra)socketResponse+=extra;
  socketResponse+="Content-Length: "+std::to_string(body.size())+"\r\nConnection: close\r\n\r\n";
  socketResponse+=body;socketState.connectOk=true;
}
static Config validConfig(){Config c={};c.version=1;c.enabled=1;c.timezoneMinutes=420;
  strcpy(c.label,"Test-Lock");strcpy(c.token,"mock_token_abcdefghijklmnopqrstuv/+=");
  strcpy(c.userId,"U0123456789abcdef0123456789abcdef");return c;}

int main(){
  fakeFreeHeap=90000;fakeLargestBlock=40000;assert(heapAdmitted());
  fakeLargestBlock=39999;assert(!heapAdmitted());
  fakeLargestBlock=40000;fakeFreeHeap=89999;assert(!heapAdmitted());
  fakeFreeHeap=200000;fakeLargestBlock=100000;
  // Actual production save/readback, input bounds, and the read-only public status.
  const char* token="mock_token_abcdefghijklmnopqrstuv/+=";
  assert(save("Host-Lock",token,"@smartlock-demo")==Result::Ok);
  Status st={};status(st);assert(st.configured&&st.enabled);assert(strcmp(st.deviceLabel,"Host-Lock")==0);
  assert(st.configVersion==2&&strcmp(st.publicBasicId,"@smartlock-demo")==0);
  {Config saved={};assert(readConfig(saved)&&saved.enabled==1&&saved.userId[0]==0&&strcmp(saved.token,token)==0);clear(&saved,sizeof(saved));}
  uint32_t noRecipientGeneration=0;{Config snap={};assert(getConfigSnapshot(snap,noRecipientGeneration));clear(&snap,sizeof(snap));}
  assert(save("bad\nlabel",token,"@ok")==Result::InvalidInput);
  assert(save("x","bad token!","@ok")==Result::InvalidInput);
  assert(save("x",token,"@invalid+id")==Result::InvalidInput);
  // The secret is confined to private config; public status has no token field and source text is not leaked.
  assert(sizeof(Status)<256);
  assert(nvs.count("sl-line:config")==0&&(nvs.count("sl-line:cfg0")==1||nvs.count("sl-line:cfg1")==1)&&nvs.count("sl-line:active")==1);
  // A label-only edit changes neither the recipient nor token. Failed candidate readback leaves the active slot intact.
  const uint32_t generationBeforeRename=gConfigGeneration;
  assert(rename("Garage Lock")==Result::Ok);status(st);assert(strcmp(st.deviceLabel,"Garage Lock")==0&&strcmp(st.publicBasicId,"@smartlock-demo")==0);
  assert(gConfigGeneration==generationBeforeRename);
  const uint8_t activeBefore=nvs["sl-line:active"][0];
  corruptReadKey="sl-line:cfg1"; // next transaction targets cfg1, and its private verify fails
  assert(rename("Rejected Rename")==Result::StorageError);
  assert(nvs["sl-line:active"][0]==activeBefore);status(st);assert(strcmp(st.deviceLabel,"Garage Lock")==0);

  // Production emitters and fixed RAM queue: FIFO, 16 maximum, drop oldest waiting.
  initLocks();refreshConfigState();
  pinFailed(1,true,false);
  Telemetry diagnostics={};telemetry(diagnostics);
  assert(diagnostics.pinGenerated==1&&diagnostics.enqueued==1);
  assert(diagnostics.heapBlocked==2&&diagnostics.pushStarted==0);
  for(int i=0;i<15;++i)emergencyUnlock();
  status(st);assert(st.queued==16&&st.failedThisRun==0);
  assert(gQueue[0].kind==EventKind::PinFailure);
  telemetry(diagnostics);assert(diagnostics.emergencyGenerated==15&&diagnostics.enqueued==16);
  emergencyUnlock();
  status(st);assert(st.queued==16&&st.failedThisRun==1);
  assert(gQueue[0].kind==EventKind::Emergency); // oldest waiting attempt was dropped
  Event inflight={};takeEvent(inflight); // exercise production dequeue without worker/network
  assert(gInFlight&&gCount==15);
  char activeRetry[37];strcpy(activeRetry,inflight.retryKey);
  for(int i=0;i<20;++i)emergencyUnlock();
  status(st);assert(st.queued==16&&st.failedThisRun==21);
  assert(gInFlight&&strcmp(inflight.retryKey,activeRetry)==0);

  // Actual production expiry sweeps stale waiting entries and counts each lost event.
  gQueue[0].enqueuedMs=millis()-60UL*60UL*1000UL-1;
  expireWaiting();status(st);assert(st.queued==15&&st.failedThisRun==22&&gInFlight);

  // Production HTTPS parser and request builder against deterministic fake TLS responses.
  Config c=validConfig();HttpResult hr={};
  setResponse(200,nullptr,"{\"sentMessages\":[{\"id\":\"opaque\"}]}");
  assert(lineRequest(c,kDeliveryPath,"{\"messages\":[]}","12345678-1234-4234-8234-123456789abc",hr));
  assert(hr.code==200);assert(socketState.request.find("POST /v2/bot/message/broadcast HTTP/1.1")!=std::string::npos);
  assert(socketState.request.find("/message/push")==std::string::npos&&socketState.request.find("\"to\"")==std::string::npos);
  assert(socketState.request.find("Authorization: Bearer mock_token_")!=std::string::npos);
  assert(socketState.request.find("X-Line-Retry-Key: 12345678-1234-4234-8234-123456789abc")!=std::string::npos);
  setResponse(409,"X-Line-Accepted-Request-Id: accepted-123\r\n","{}");
  assert(lineRequest(c,kDeliveryPath,"{}","12345678-1234-4234-8234-123456789abc",hr)&&hr.code==409&&hr.acceptedConflict);
  setResponse(409,"X-Line-Request-Id: arbitrary\r\n","{}");
  assert(lineRequest(c,kDeliveryPath,"{}","12345678-1234-4234-8234-123456789abc",hr)&&!hr.acceptedConflict);
  const int codes[]={400,401,403,429,500,503};
  for(int code:codes){setResponse(code,nullptr,"{}");assert(lineRequest(c,kDeliveryPath,"{}","12345678-1234-4234-8234-123456789abc",hr));assert(hr.code==code);}
  setResponse(200,nullptr,"{}");
  // Declared response bytes must arrive; incomplete data is rejected.
  socketResponse="HTTP/1.1 200 OK\r\nContent-Length: 20\r\n\r\n{}";
  assert(!lineRequest(c,kDeliveryPath,"{}","12345678-1234-4234-8234-123456789abc",hr));
  // A complete, valid chunked response passes; malformed JSON is rejected.
  socketResponse="HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n7\r\n{\"x\":1}\r\n0\r\n\r\n";
  assert(lineRequest(c,kDeliveryPath,"{}","12345678-1234-4234-8234-123456789abc",hr)&&hr.code==200);
  setResponse(200,nullptr,"{garbage}");
  assert(!lineRequest(c,kDeliveryPath,"{}","12345678-1234-4234-8234-123456789abc",hr));
  socketState.connectOk=false;
  assert(!lineRequest(c,kDeliveryPath,"{}","12345678-1234-4234-8234-123456789abc",hr));
  socketState.connectOk=true;

  // Quota response fields are parsed from the production parser and capped at the Free-plan limit.
  setResponse(200,nullptr,"{\"type\":\"limited\",\"value\":300}");
  assert(lineRequest(c,"/v2/bot/message/quota",nullptr,nullptr,hr)&&hr.quotaKnown&&hr.quota==300);
  setResponse(200,nullptr,"{\"type\":\"limited\",\"value\":500}");
  assert(lineRequest(c,"/v2/bot/message/quota",nullptr,nullptr,hr)&&!hr.quotaKnown);
  setResponse(200,nullptr,"{\"totalUsage\":299}");
  assert(lineRequest(c,"/v2/bot/message/quota/consumption",nullptr,nullptr,hr)&&hr.usageKnown&&hr.quotaUsed==299);

  // The real test route rate limiter records only a queued test and never reveals credentials.
  finishEvent(false); // conclude the synthetic in-flight event from the queue test
  assert(requestTest()==Result::Ok);assert(requestTest()==Result::RateLimited);
  status(st);assert(st.testState==1);
  Event testMessage={};testMessage.kind=EventKind::Test;strcpy(testMessage.label,"Host-Lock");
  char testText[1025]={};assert(buildText(testMessage,420,testText));
  assert(strstr(testText,"🔔 ทดสอบการแจ้งเตือน SmartLock")!=nullptr);
  assert(strstr(testText,"อุปกรณ์: Host-Lock")!=nullptr);
  assert(strstr(testText,"ระบบแจ้งเตือน LINE พร้อมใช้งาน")!=nullptr);
  clear(&c,sizeof(c));
  assert(disconnect()==Result::Ok);status(st);assert(!st.configured&&!st.enabled&&st.queued==0&&strcmp(st.publicBasicId,"@smartlock-demo")==0);
  for(size_t i=0;i<sizeof(gConfigCache.token);++i)assert(gConfigCache.token[i]==0);
  assert(nvs.count("sl-line:config")==0);
  assert(nvs.count("sl-line:cfg0")==0||nvs.count("sl-line:cfg1")==0);
  const char* disabledKey=nvs.count("sl-line:cfg0")?"sl-line:cfg0":"sl-line:cfg1";
  Config disabled={};memcpy(&disabled,nvs[disabledKey].data(),sizeof(disabled));
  assert(!disabled.enabled&&disabled.token[0]==0&&disabled.userId[0]==0&&strcmp(disabled.publicBasicId,"@smartlock-demo")==0);

  // Exact v1 binary layout migrates without losing config and leaves public Basic ID empty.
  nvs.clear();ConfigV1 legacy={};legacy.version=1;legacy.enabled=1;legacy.timezoneMinutes=420;
  strcpy(legacy.label,"Legacy Device");strcpy(legacy.token,token);strcpy(legacy.userId,"U0123456789abcdef0123456789abcdef");
  Preferences oldPrefs;assert(oldPrefs.begin("sl-line",false));assert(oldPrefs.putBytes("config",&legacy,sizeof(legacy))==sizeof(legacy));oldPrefs.end();
  refreshConfigState();status(st);
  assert(st.configured&&st.configVersion==2&&strcmp(st.deviceLabel,"Legacy Device")==0&&st.publicBasicId[0]==0);
  assert(nvs.count("sl-line:active")==1&&(nvs.count("sl-line:cfg0")==1||nvs.count("sl-line:cfg1")==1));
  Config migrated={};assert(readConfig(migrated)&&migrated.version==2&&migrated.enabled&&migrated.publicBasicId[0]==0);
  uint32_t migrationGeneration=0;Config workerSnapshot={};assert(getConfigSnapshot(workerSnapshot,migrationGeneration)&&migrationGeneration==gConfigGeneration);
  assert(strcmp(migrated.token,token)==0&&strcmp(migrated.userId,"U0123456789abcdef0123456789abcdef")==0);
  clear(&migrated,sizeof(migrated));clear(&legacy,sizeof(legacy));
  // Factory Reset (prototype policy): runtime stop only; persisted sl-line survives byte-for-byte.
  nvs.clear();gSuspended=false;
  assert(save("Reset-Lock",token,"@smartlock-demo")==Result::Ok);
  pinFailed(1,true,false);status(st);assert(st.configured&&st.enabled&&st.queued>0);
  const auto beforeReset=nvs;const uint32_t generationBeforeReset=gConfigGeneration;
  assert(suspendForReset()==Result::Ok);status(st);
  assert(!st.configured&&!st.enabled&&st.queued==0&&st.state==State::Disabled);
  assert(gConfigGeneration!=generationBeforeReset);
  for(size_t i=0;i<sizeof(gConfigCache.token);++i)assert(gConfigCache.token[i]==0);
  uint32_t suspendedGeneration=0;Config suspendedSnapshot={};assert(!getConfigSnapshot(suspendedSnapshot,suspendedGeneration));
  assert(nvs==beforeReset);
  // While suspended nothing re-enables delivery or mutates persisted config.
  pinFailed(2,true,false);emergencyUnlock();unlockSuccess("U1","Owner","Owner","access");
  assert(requestTest()==Result::NotConfigured);
  assert(save("Other",token,"@other")==Result::StorageError);
  assert(rename("Other")==Result::StorageError);
  assert(disconnect()==Result::StorageError);
  refreshConfigState();status(st);assert(!st.enabled&&st.queued==0);
  assert(nvs==beforeReset);
  // Simulated reboot: RAM state is fresh, preserved config loads normally.
  gSuspended=false;refreshConfigState();status(st);
  assert(st.configured&&st.enabled&&st.configVersion==2);
  assert(strcmp(st.deviceLabel,"Reset-Lock")==0&&strcmp(st.publicBasicId,"@smartlock-demo")==0);
  Config preserved={};assert(readConfig(preserved)&&preserved.version==2&&preserved.enabled==1&&preserved.timezoneMinutes==420);
  assert(strcmp(preserved.token,token)==0&&preserved.userId[0]==0&&strcmp(preserved.publicBasicId,"@smartlock-demo")==0);
  assert(nvs.count("sl-line:active")==1);
  clear(&preserved,sizeof(preserved));
  // Delivery endpoint and body: every notification kind is a broadcast with no "to" recipient.
  assert(strcmp(kDeliveryPath,"/v2/bot/message/broadcast")==0);
  {const EventKind kinds[]={EventKind::Unlock,EventKind::PinFailure,EventKind::Emergency,EventKind::Test};
   for(EventKind kind:kinds){Event e={};e.kind=kind;strcpy(e.label,"Broadcast-Lock");strcpy(e.userId,"D000001");strcpy(e.identityName,"Owner");strcpy(e.role,"Owner");strcpy(e.source,"access");e.wrongCount=1;e.locked=true;
    char text[1025]={},body[2048]={};assert(buildDeliveryBody(e,420,text,body,sizeof(body)));
    {const char* prefix="{\"messages\":[{\"type\":\"text\",\"text\":\"";assert(strncmp(body,prefix,strlen(prefix))==0);}
    assert(strstr(body,"\"to\"")==nullptr&&strstr(body,"U0123456789abcdef")==nullptr);
    assert(strstr(body,"Broadcast-Lock")!=nullptr);assert(body[strlen(body)-1]=='}');}}
  // Deployed v2 config written by the fixed-recipient firmware (recipient present) still loads:
  // token/Basic ID/enabled kept, recipient ignored for delivery, no migration write needed.
  nvs.clear();gSuspended=false;
  {Config v2={};v2.version=2;v2.enabled=1;v2.timezoneMinutes=420;strcpy(v2.label,"Deployed-Lock");strcpy(v2.token,token);
   strcpy(v2.userId,"U0123456789abcdef0123456789abcdef");strcpy(v2.publicBasicId,"@smartlock-demo");
   Preferences pv;assert(pv.begin("sl-line",false));assert(pv.putBytes("cfg0",&v2,sizeof(v2))==sizeof(v2));assert(pv.putUChar("active",0)==1);pv.end();
   const auto before=nvs;refreshConfigState();status(st);
   assert(st.configured&&st.enabled&&st.configVersion==2&&strcmp(st.publicBasicId,"@smartlock-demo")==0);
   assert(nvs==before);
   Config loaded={};assert(readConfig(loaded)&&strcmp(loaded.token,token)==0&&strcmp(loaded.publicBasicId,"@smartlock-demo")==0);clear(&loaded,sizeof(loaded));
   // Factory Reset suspend keeps that deployed config byte-for-byte too.
   assert(suspendForReset()==Result::Ok);assert(nvs==before);gSuspended=false;refreshConfigState();status(st);assert(st.configured&&st.enabled);
   clear(&v2,sizeof(v2));}
  // A malformed stored recipient still invalidates the slot (structural integrity is unchanged).
  {Config bad={};bad.version=2;bad.enabled=1;bad.timezoneMinutes=420;strcpy(bad.label,"Bad");strcpy(bad.token,token);strcpy(bad.userId,"garbage");assert(!validateConfig(bad));
   bad.userId[0]=0;assert(validateConfig(bad));bad.token[0]=0;assert(!validateConfig(bad));bad.enabled=0;assert(validateConfig(bad));}
  puts("LINE notification host fake tests: PASS");
  return 0;
}
