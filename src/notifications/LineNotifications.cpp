#include "LineNotifications.h"
#include "LineProtocol.h"
#include <atomic>
#include <esp_heap_caps.h>
#ifndef LINE_NOTIFICATIONS_HOST_TEST
#include <lwip/dns.h>
#endif

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_system.h>
#include <time.h>
#include <strings.h>
#include <stdlib.h>
#include "../events/TimeManager.h"
#include <string.h>

namespace LineNotifications {
namespace {

constexpr uint8_t kQueueCapacity = 16;
// Every notification kind (unlock, PIN failure, emergency, test) is a broadcast.
constexpr const char* kDeliveryPath = "/v2/bot/message/broadcast";
constexpr uint32_t kTtlMs = 60UL * 60UL * 1000UL;
constexpr uint32_t kTestIntervalMs = 60UL * 1000UL;
constexpr uint8_t kMaxAttempts = 20;
constexpr size_t kTokenMax = 512;
constexpr char kFallbackLabel[] = "SmartLock-04225A0FF0A4";
constexpr char kHost[] = "api.line.me";
constexpr char kRootCa[] =
"-----BEGIN CERTIFICATE-----\n"
"MIICPzCCAcWgAwIBAgIQBVVWvPJepDU1w6QP1atFcjAKBggqhkjOPQQDAzBhMQsw\n"
"CQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cu\n"
"ZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBHMzAe\n"
"Fw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVTMRUw\n"
"EwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5jb20x\n"
"IDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEczMHYwEAYHKoZIzj0CAQYF\n"
"K4EEACIDYgAE3afZu4q4C/sLfyHS8L6+c/MzXRq8NOrexpu80JX28MzQC7phW1FG\n"
"fp4tn+6OYwwX7Adw9c+ELkCDnOg/QW07rdOkFFk2eJ0DQ+4QE2xy3q6Ip6FrtUPO\n"
"Z9wj/wMco+I+o0IwQDAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAd\n"
"BgNVHQ4EFgQUs9tIpPmhxdiuNkHMEWNpYim8S8YwCgYIKoZIzj0EAwMDaAAwZQIx\n"
"AK288mw/EkrRLTnDCgmXc/SINoyIJ7vmiI1Qhadj+Z4y3maTD/HMsQmP3Wyr+mt/\n"
"oAIwOWZbwmSNuJ5Q3KjVSaLtx9zRSX8XAbjIho9OjIgrqJqpisXRAL34VOKa5Vt8\n"
"sycX\n"
"-----END CERTIFICATE-----\n";

enum class EventKind : uint8_t { Unlock, PinFailure, Emergency, Test };
struct Event {
  EventKind kind;
  uint32_t enqueuedMs;
  uint32_t epoch;
  uint8_t wrongCount;
  bool locked;
  bool lockout;
  char label[65];
  char userId[33];
  char identityName[65];
  char role[24];
  char source[24];
  char retryKey[37];
};
struct Config {
  uint16_t version;
  uint8_t enabled;
  int16_t timezoneMinutes;
  char label[65];
  char token[513];
  // Deprecated (fixed-recipient push era). Kept only so the v2 binary layout
  // and existing A/B slots stay readable; ignored for delivery, never rendered,
  // written empty by save(). Delivery is a broadcast to all OA friends.
  char userId[34];
  char publicBasicId[65];
};
struct ConfigV1 {
  uint16_t version;
  uint8_t enabled;
  int16_t timezoneMinutes;
  char label[65];
  char token[513];
  char userId[34];
};

Event gQueue[kQueueCapacity];
uint8_t gCount = 0;
bool gInFlight = false;
std::atomic<bool> gSta{false};
bool gConfigured = false;
std::atomic<bool> gEnabled{false};
uint16_t gConfigVersion = 0;
uint32_t gSent = 0;
uint32_t gFailed = 0;
uint32_t gLastTestMs = 0;
bool gTestStarted = false;
std::atomic<bool> gQuotaKnown{false};
std::atomic<bool> gQuotaFull{false};
uint32_t gConfigGeneration = 1;
Config gConfigCache = {};
char gLabelCache[65] = {};
std::atomic<uint16_t> gQuotaLimit{0};
std::atomic<uint16_t> gQuotaUsed{0};
State gState = State::Disabled;
std::atomic<bool> gAuthBlocked{false};
// Set once by Factory Reset. RAM only: blocks re-enable and persistent writes until reboot.
std::atomic<bool> gSuspended{false};
std::atomic<uint8_t> gTestState{0};
uint32_t gQuotaPeriod=0;
SemaphoreHandle_t gQueueMutex = nullptr;
SemaphoreHandle_t gConfigMutex = nullptr;
SemaphoreHandle_t gWake = nullptr;
TaskHandle_t gTask = nullptr;
std::atomic<uint32_t> gUnlockGenerated{0},gPinGenerated{0},gEmergencyGenerated{0},gEnqueued{0};
std::atomic<uint32_t> gPushStarted{0},gTlsConnected{0},gPushWritten{0},gResponses{0},gHeapBlocked{0};
std::atomic<uint32_t> gAdmissionFree{0},gAdmissionLargest{0},gAdmissionMinimum{0};
std::atomic<uint32_t> gTlsMinimum{0},gTlsAfterLargest{0};
std::atomic<uint16_t> gLastPushHttp{0};
bool heapAdmitted(){
  gAdmissionFree=ESP.getFreeHeap();gAdmissionLargest=heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);gAdmissionMinimum=ESP.getMinFreeHeap();
  const bool admitted=gAdmissionFree>=90000&&gAdmissionLargest>=40000;
  if(!admitted)++gHeapBlocked;return admitted;
}
struct TlsMeasurement {
  ~TlsMeasurement(){gTlsMinimum=ESP.getMinFreeHeap();gTlsAfterLargest=heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);}
};


void clear(void* p, size_t n) { volatile uint8_t* b = static_cast<volatile uint8_t*>(p); while (n--) *b++ = 0; }
void satInc(uint32_t& v) { if (v != UINT32_MAX) ++v; }
size_t boundedLen(const char* s, size_t max) { return s ? strnlen(s, max + 1) : 0; }
bool validUtf8(const char* s, size_t n);

bool validLabel(const char* s) {
  if (!s) return true;
  const size_t n = boundedLen(s, 64);
  if (n > 64) return false;
  for (size_t i = 0; i < n; ++i) if (static_cast<uint8_t>(s[i]) < 0x20 || s[i] == 0x7f) return false;
  return validUtf8(s, n);
}
bool validToken(const char* s) {
  const size_t n = boundedLen(s, kTokenMax);
  if (n < 20 || n > kTokenMax) return false;
  for (size_t i = 0; i < n; ++i) {
    const char c = s[i];
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '=' || c == '+' || c == '/')) return false;
  }
  return true;
}
bool validUserId(const char* s) {
  if (!s || boundedLen(s, 34) != 33 || s[0] != 'U') return false;
  for (size_t i = 1; i < 33; ++i) {
    const char c = s[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
  }
  return true;
}
bool validBasicId(const char* s) {
  const size_t n=boundedLen(s,64);
  if(!s||n<2||n>64||s[0]!='@')return false;
  for(size_t i=1;i<n;++i){const char c=s[i];if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
    (c>='0'&&c<='9')||c=='.'||c=='_'||c=='-'))return false;}
  return true;
}
bool validUtf8(const char* s, size_t n) {
  size_t i = 0;
  while (i < n) {
    uint8_t c = static_cast<uint8_t>(s[i++]);
    if (c < 0x80) continue;
    uint32_t cp; size_t extra;
    if (c >= 0xC2 && c <= 0xDF) { cp = c & 0x1F; extra = 1; }
    else if (c >= 0xE0 && c <= 0xEF) { cp = c & 0x0F; extra = 2; }
    else if (c >= 0xF0 && c <= 0xF4) { cp = c & 0x07; extra = 3; }
    else return false;
    if (i + extra > n) return false;
    for (size_t j = 0; j < extra; ++j) {
      uint8_t d = static_cast<uint8_t>(s[i++]);
      if ((d & 0xC0) != 0x80) return false;
      cp = (cp << 6) | (d & 0x3F);
    }
    if ((extra == 1 && cp < 0x80) || (extra == 2 && cp < 0x800) || (extra == 3 && cp < 0x10000) ||
        cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
  }
  return true;
}
void makeUuid(char out[37]) {
  uint8_t b[16];
  for (size_t i = 0; i < sizeof(b); i += 4) { uint32_t r = esp_random(); memcpy(b + i, &r, 4); }
  b[6] = (b[6] & 0x0F) | 0x40; b[8] = (b[8] & 0x3F) | 0x80;
  snprintf(out, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);
  clear(b, sizeof(b));
}
uint32_t currentEpoch() { return TimeManager::epochSeconds(); }

bool validateConfig(const Config& c) {
  return c.version==2&&c.enabled<=1&&c.timezoneMinutes>=-720&&c.timezoneMinutes<=840&&
    c.label[64]==0&&c.token[512]==0&&c.userId[33]==0&&c.publicBasicId[64]==0&&
    validLabel(c.label)&&(!c.publicBasicId[0]||validBasicId(c.publicBasicId))&&
    (!c.userId[0]||validUserId(c.userId))&&(!c.enabled||validToken(c.token));
}
bool readConfig(Config& c,bool* needsMigration=nullptr) {
  memset(&c, 0, sizeof(c));
  if(needsMigration)*needsMigration=false;
  Preferences p;
  if (!p.begin("sl-line", true)) return false;
  const uint8_t slot=p.getUChar("active",0xff);
  bool ok=false;
  if(slot<=1){
    const char* key=slot==0?"cfg0":"cfg1";
    ok=p.getBytesLength(key)==sizeof(c)&&p.getBytes(key,&c,sizeof(c))==sizeof(c)&&validateConfig(c);
  }
  if(!ok&&slot==0xff){
    ConfigV1 old={};
    if(p.getBytesLength("config")==sizeof(old)&&p.getBytes("config",&old,sizeof(old))==sizeof(old)&&
       old.version==1&&old.enabled<=1&&old.label[64]==0&&old.token[512]==0&&old.userId[33]==0&&
       validLabel(old.label)&&(!old.userId[0]||validUserId(old.userId))&&(!old.enabled||validToken(old.token))){
      c.version=2;c.enabled=old.enabled;c.timezoneMinutes=old.timezoneMinutes;
      memcpy(c.label,old.label,sizeof(c.label));memcpy(c.token,old.token,sizeof(c.token));memcpy(c.userId,old.userId,sizeof(c.userId));
      if(needsMigration)*needsMigration=true;ok=true;
    }
    clear(&old,sizeof(old));
  }
  p.end();
  if (!ok || !validateConfig(c)) {
    clear(&c, sizeof(c));
    return false;
  }
  return true;
}
bool writeConfig(const Config& c) {
  if(!validateConfig(c))return false;
  Preferences p;
  uint8_t oldSlot=0xff;
  if(p.begin("sl-line",true)){oldSlot=p.getUChar("active",0xff);p.end();}
  const uint8_t target=oldSlot==0?1:(oldSlot==1?0:1);
  if (!p.begin("sl-line", false)) return false;
  const char* targetKey=target==0?"cfg0":"cfg1";
  const bool ok = p.putBytes(targetKey, &c, sizeof(c)) == sizeof(c);
  p.end();
  if (!ok) return false;
  Config verify={};
  Preferences check;
  bool same=check.begin("sl-line",true)&&check.getBytesLength(targetKey)==sizeof(verify)&&
    check.getBytes(targetKey,&verify,sizeof(verify))==sizeof(verify)&&validateConfig(verify)&&
    memcmp(&verify,&c,sizeof(c))==0;
  check.end();
  clear(&verify,sizeof(verify));
  if(!same)return false;
  Preferences commit;
  if(!commit.begin("sl-line",false))return false;
  const bool selected=commit.putUChar("active",target)==1;
  const uint8_t selectedSlot=commit.getUChar("active",0xff);
  commit.end();
  if(!selected||selectedSlot!=target){
    // The previous active slot remains intact; restore its selector if a partial commit occurred.
    if(commit.begin("sl-line",false)){if(oldSlot==0xff)commit.remove("active");else commit.putUChar("active",oldSlot);commit.end();}
    return false;
  }
  Config active={};
  const bool readBack=readConfig(active)&&memcmp(&active,&c,sizeof(c))==0;
  clear(&active,sizeof(active));
  if(!readBack){
    Preferences rollback;
    if(rollback.begin("sl-line",false)){if(oldSlot==0xff)rollback.remove("active");else rollback.putUChar("active",oldSlot);rollback.end();}
    return false;
  }
  return true;
}
void refreshConfigState() {
  if(gSuspended)return;
  Config c={};bool migration=false;
  bool ok = readConfig(c,&migration);
  uint16_t storageVersion=ok?(migration?1:2):0;
  if(ok&&migration&&writeConfig(c))storageVersion=2;
  if (gConfigMutex) xSemaphoreTake(gConfigMutex, portMAX_DELAY);
  gConfigured = ok && c.enabled;
  gEnabled = ok && c.enabled;
  gConfigVersion=storageVersion;
  clear(&gConfigCache, sizeof(gConfigCache));
  if (ok) gConfigCache = c;
  strlcpy(gLabelCache, (ok && c.label[0]) ? c.label : kFallbackLabel, sizeof(gLabelCache));
  ++gConfigGeneration;
  if (!gConfigGeneration) ++gConfigGeneration;
  gQuotaKnown=false;gQuotaFull=false;gQuotaLimit=gQuotaUsed=0;gQuotaPeriod=0;gAuthBlocked=false;gTestState=0;
  gState = gConfigured ? State::TimeUnavailable : State::Disabled;
  clear(&c, sizeof(c));
  if (gConfigMutex) xSemaphoreGive(gConfigMutex);
}

void enqueue(Event& e) {
  if (!gQueueMutex) return;
  xSemaphoreTake(gQueueMutex, portMAX_DELAY);
  if (!gEnabled) { xSemaphoreGive(gQueueMutex); clear(&e, sizeof(e)); return; }
  if (static_cast<uint8_t>(gCount + (gInFlight ? 1 : 0)) >= kQueueCapacity && gCount) {
    if(gQueue[0].kind==EventKind::Test)gTestState=3;
    clear(&gQueue[0], sizeof(Event));
    memmove(gQueue, gQueue + 1, sizeof(Event) * (gCount - 1));
    --gCount;
    satInc(gFailed);
  } else if (static_cast<uint8_t>(gCount + (gInFlight ? 1 : 0)) >= kQueueCapacity) {
    satInc(gFailed); xSemaphoreGive(gQueueMutex); clear(&e, sizeof(e)); return;
  }
  gQueue[gCount++] = e; ++gEnqueued;
  clear(&e, sizeof(e));
  xSemaphoreGive(gQueueMutex);
  if (gWake) xSemaphoreGive(gWake);
}

size_t append(char* dst,size_t cap,size_t at,const char* text) {
  if(at>=cap)return SIZE_MAX;
  size_t n=strlen(text);if(n>=cap-at)return SIZE_MAX;
  memcpy(dst+at,text,n+1);return at+n;
}
size_t appendEscaped(char* dst,size_t cap,size_t at,const char* text) {
  if(at>=cap)return SIZE_MAX;
  for(const unsigned char* p=(const unsigned char*)text;*p;++p){
    const unsigned char c=*p;char unit[7]={};size_t n=1;unit[0]=c;
    if(c=='"'||c=='\\'){unit[0]='\\';unit[1]=c;n=2;}
    else if(c<32){snprintf(unit,sizeof(unit),"\\u%04x",c);n=6;}
    if(n>=cap-at)return SIZE_MAX;memcpy(dst+at,unit,n);at+=n;
  }dst[at]=0;return at;
}
void getTimeText(const Event& e, char* out, size_t cap, int16_t offsetMinutes) {
  if (!e.epoch) { strlcpy(out, "ยังไม่ยืนยันเวลา", cap); return; }
  time_t adjusted = static_cast<time_t>(e.epoch + static_cast<int32_t>(offsetMinutes) * 60);
  struct tm tmv;
  if (!gmtime_r(&adjusted, &tmv)) { strlcpy(out, "ยังไม่ยืนยันเวลา", cap); return; }
  snprintf(out, cap, "%02d/%02d/%04d %02d:%02d:%02d (UTC%+03d:%02d)",
    tmv.tm_mday, tmv.tm_mon + 1, tmv.tm_year + 1900, tmv.tm_hour, tmv.tm_min, tmv.tm_sec,
    offsetMinutes / 60, abs(offsetMinutes % 60));
}
bool buildText(const Event& e, int16_t offsetMinutes, char out[1025]) {
  char t[48]; getTimeText(e, t, sizeof(t), offsetMinutes);
  char age[72] = "";
  const uint32_t delay = millis() - e.enqueuedMs;
  if (delay > 30000) snprintf(age, sizeof(age), "\nแจ้งเตือนล่าช้า %lu วินาที", static_cast<unsigned long>(delay / 1000));
  size_t n = 0; out[0] = 0;
  switch (e.kind) {
    case EventKind::Unlock:
      n=append(out,1025,n,"🔓 SmartLock ปลดล็อกสำเร็จ\nอุปกรณ์: "); n=append(out,1025,n,e.label);
      n=append(out,1025,n,"\nเวลา: ");n=append(out,1025,n,t);n=append(out,1025,n,"\nผู้ใช้งาน: ");n=append(out,1025,n,e.identityName);
      n=append(out,1025,n,"\nรหัสผู้ใช้งาน: ");n=append(out,1025,n,e.userId);n=append(out,1025,n,"\nสิทธิ์: ");n=append(out,1025,n,e.role);
      n=append(out,1025,n,"\nวิธี: Access QR / ");n=append(out,1025,n,e.source);n=append(out,1025,n,"\nผล: สั่งปลดล็อกสำเร็จ");break;
    case EventKind::PinFailure:
      n=append(out,1025,n,"⚠️ ตรวจพบการใส่รหัส Admin ผิด\nอุปกรณ์: ");n=append(out,1025,n,e.label);
      n=append(out,1025,n,"\nเวลา: ");n=append(out,1025,n,t);n=append(out,1025,n,"\nผิดครั้งที่: ");
      { char c[8];snprintf(c,sizeof(c),"%u",e.wrongCount);n=append(out,1025,n,c); }
      n=append(out,1025,n,"\nสถานะ: ");n=append(out,1025,n,e.locked?"ล็อกอยู่":"ปลดล็อกอยู่");
      if(e.lockout)n=append(out,1025,n,"\nระงับการลองรหัสชั่วคราว");break;
    case EventKind::Emergency:
      n=append(out,1025,n,"🚨 SmartLock ปลดล็อกฉุกเฉิน\nอุปกรณ์: ");n=append(out,1025,n,e.label);
      n=append(out,1025,n,"\nเวลา: ");n=append(out,1025,n,t);n=append(out,1025,n,"\nวิธี: เมนู Admin บนอุปกรณ์\nผู้ดำเนินการ: ไม่ระบุตัวบุคคล\nผล: สั่งปลดล็อกสำเร็จ");break;
    case EventKind::Test:
      n=append(out,1025,n,"\U0001F514 \u0e17\u0e14\u0e2a\u0e2d\u0e1a\u0e01\u0e32\u0e23\u0e41\u0e08\u0e49\u0e07\u0e40\u0e15\u0e37\u0e2d\u0e19 SmartLock\n\u0e2d\u0e38\u0e1b\u0e01\u0e23\u0e13\u0e4c: ");n=append(out,1025,n,e.label);
      n=append(out,1025,n,"\n\u0e40\u0e27\u0e25\u0e32: ");n=append(out,1025,n,t);n=append(out,1025,n,"\n\u0e23\u0e30\u0e1a\u0e1a\u0e41\u0e08\u0e49\u0e07\u0e40\u0e15\u0e37\u0e2d\u0e19 LINE \u0e1e\u0e23\u0e49\u0e2d\u0e21\u0e43\u0e0a\u0e49\u0e07\u0e32\u0e19");break;
  }
  n=append(out,1025,n,age);
  return n < 1024;
}

bool writeAll(WiFiClientSecure& client, const char* p, size_t n, uint32_t deadline) {
  while (n && static_cast<int32_t>(millis() - deadline) < 0) {
    if (!client.connected()) return false;
    size_t w = client.write(reinterpret_cast<const uint8_t*>(p), n);
    if (w) { p += w; n -= w; }
    else vTaskDelay(pdMS_TO_TICKS(10));
  }
  return n == 0;
}
bool readLine(WiFiClientSecure& c, char* out, size_t cap, uint32_t deadline) {
  size_t n=0;
  while (static_cast<int32_t>(millis()-deadline)<0 && n+1<cap) {
    while(c.available()) { int ch=c.read(); if(ch<0)break; if(ch=='\n'){out[n]=0; if(n&&out[n-1]=='\r')out[n-1]=0; return true;} if(n+1>=cap)return false;out[n++]=static_cast<char>(ch); }
    if (!c.connected() && !c.available()) break;
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  out[n]=0; return false;
}
struct HttpResult { int code; bool acceptedConflict; uint16_t quota,quotaUsed; bool quotaKnown,usageKnown,quotaFull; };
#ifndef LINE_NOTIFICATIONS_HOST_TEST
std::atomic<uint32_t> resolvedIp{0};
void dnsFound(const char*,const ip_addr_t* address,void*) {
  if(address && IP_IS_V4(address))resolvedIp.store(ip4_addr_get_u32(ip_2_ip4(address)));
}
bool connectBounded(WiFiClientSecure& client,uint32_t deadline) {
  resolvedIp=0;ip_addr_t address;
  const err_t err=dns_gethostbyname(kHost,&address,dnsFound,nullptr);
  if(err==ERR_OK && IP_IS_V4(&address))resolvedIp=ip4_addr_get_u32(ip_2_ip4(&address));
  else if(err!=ERR_INPROGRESS)return false;
  const uint32_t dnsStart=millis();
  while(!resolvedIp && millis()-dnsStart<3000 && static_cast<int32_t>(millis()-deadline)<0)vTaskDelay(pdMS_TO_TICKS(10));
  if(!resolvedIp)return false;
  // Explicit hostname retains SNI and certificate hostname validation after bounded DNS.
  return client.connect(IPAddress(resolvedIp.load()),443,kHost,kRootCa,nullptr,nullptr);
}
#else
bool connectBounded(WiFiClientSecure& client,uint32_t){return client.connect(kHost,443,1000);}
#endif
bool readBodyBytes(WiFiClientSecure& c,char* output,size_t length,uint32_t deadline){
  size_t used=0;
  while(used<length && static_cast<int32_t>(millis()-deadline)<0){
    if(c.available()){int n=c.read(reinterpret_cast<uint8_t*>(output)+used,length-used);if(n>0)used+=n;}
    else if(!c.connected())return false;else vTaskDelay(pdMS_TO_TICKS(5));
  }return used==length;
}
bool lineRequest(const Config& cfg,const char* path,const char* body,const char* retryKey,HttpResult& result){
  result={0,false,0,0,false,false,false};TlsMeasurement measurement;WiFiClientSecure client;
  if(body){++gPushStarted;gLastPushHttp=0;}
  client.setCACert(kRootCa);client.setHandshakeTimeout(5);client.setTimeout(1);
  const uint32_t deadline=millis()+12000;
  if(!connectBounded(client,deadline)||static_cast<int32_t>(millis()-deadline)>=0){client.stop();return false;}
  ++gTlsConnected;
  char head[1200];
  int n=body ? snprintf(head,sizeof(head),"POST %s HTTP/1.1\r\nHost: %s\r\nAuthorization: Bearer %s\r\nContent-Type: application/json\r\nContent-Length: %u\r\nConnection: close\r\nX-Line-Retry-Key: %s\r\n\r\n",path,kHost,cfg.token,(unsigned)strlen(body),retryKey)
    : snprintf(head,sizeof(head),"GET %s HTTP/1.1\r\nHost: %s\r\nAuthorization: Bearer %s\r\nConnection: close\r\n\r\n",path,kHost,cfg.token);
  bool ok=n>0 && static_cast<size_t>(n)<sizeof(head) && writeAll(client,head,n,deadline);
  clear(head,sizeof(head));if(ok&&body)ok=writeAll(client,body,strlen(body),deadline);
  if(ok&&body)++gPushWritten;
  char line[256];if(!ok||!readLine(client,line,sizeof(line),deadline)){client.stop();return false;}
  if(strncmp(line,"HTTP/1.1 ",9)&&strncmp(line,"HTTP/1.0 ",9)){client.stop();return false;}
  if(strlen(line)<12||line[9]<'1'||line[9]>'5'||line[10]<'0'||line[10]>'9'||line[11]<'0'||line[11]>'9'||(line[12]&&line[12]!=' ')){client.stop();return false;}
  ++gResponses;result.code=(line[9]-'0')*100+(line[10]-'0')*10+line[11]-'0';if(body)gLastPushHttp=result.code;
  bool accepted=false,complete=false,chunked=false,hasLength=false;uint32_t length=0,totalHeaders=0;
  while(readLine(client,line,sizeof(line),deadline)){
    totalHeaders+=strlen(line)+2;if(totalHeaders>2048){client.stop();return false;}
    if(!line[0]){complete=true;break;}
    if(!strncasecmp(line,"X-Line-Accepted-Request-Id:",27))accepted=LineProtocol::acceptedId(line+27);
    if(!strncasecmp(line,"Content-Length:",15)){
      const char* value=line+15;while(*value==' '||*value=='\t')++value;
      if(hasLength||!LineProtocol::decimal(value,length)||length>2048){client.stop();return false;}hasLength=true;
    }
    if(!strncasecmp(line,"Transfer-Encoding:",18)){
      const char* value=line+18;while(*value==' '||*value=='\t')++value;
      if(strcasecmp(value,"chunked")){client.stop();return false;}chunked=true;
    }
  }
  if(!complete||(hasLength&&chunked)){client.stop();return false;}
  char response[2049]={};size_t used=0;
  if(chunked){
    for(;;){
      if(!readLine(client,line,sizeof(line),deadline)){ok=false;break;}
      const char* p=line;uint32_t chunk=0;unsigned digits=0;
      for(;*p;++p){unsigned d;if(*p>='0'&&*p<='9')d=*p-'0';else if(*p>='a'&&*p<='f')d=*p-'a'+10;else if(*p>='A'&&*p<='F')d=*p-'A'+10;else{ok=false;break;}
        if(++digits>8||chunk>2048/16){ok=false;break;}chunk=chunk*16+d;}
      if(!ok||!digits||chunk>2048-used){ok=false;break;}
      if(!chunk){if(!readLine(client,line,sizeof(line),deadline)||line[0])ok=false;break;}
      if(!readBodyBytes(client,response+used,chunk,deadline)){ok=false;break;}used+=chunk;
      char crlf[2];if(!readBodyBytes(client,crlf,2,deadline)||crlf[0]!='\r'||crlf[1]!='\n'){ok=false;break;}
    }
  }else if(hasLength){ok=readBodyBytes(client,response,length,deadline);used=length;}
  else{
    while(static_cast<int32_t>(millis()-deadline)<0){
      if(client.available()){if(used==2048){ok=false;break;}int c=client.read();if(c>=0)response[used++]=c;}
      else if(!client.connected())break;else vTaskDelay(pdMS_TO_TICKS(5));
    }
    if(client.connected())ok=false;
  }
  response[used]=0;
  if(ok){ok=!memchr(response,0,used)&&validUtf8(response,used)&&LineProtocol::jsonObject(response);}
  if(ok){
    result.acceptedConflict=result.code==409&&accepted;
    if(!strcmp(path,"/v2/bot/message/quota")&&result.code==200){
      uint16_t value=0;if(LineProtocol::numberField(response,"value",value)&&value<=300&&LineProtocol::limitedQuota(response)){result.quota=value;result.quotaKnown=true;}
    }
    if(!strcmp(path,"/v2/bot/message/quota/consumption")&&result.code==200)result.usageKnown=LineProtocol::numberField(response,"totalUsage",result.quotaUsed);
  }
  clear(response,sizeof(response));client.stop();return ok;
}

void updateState(State state) {
  if(gConfigMutex)xSemaphoreTake(gConfigMutex,portMAX_DELAY);
  gState=state;
  if(gConfigMutex)xSemaphoreGive(gConfigMutex);
}
bool takeEvent(Event& e) {
  xSemaphoreTake(gQueueMutex,portMAX_DELAY);
  bool ok=gCount>0;
  if(ok){e=gQueue[0]; if(gCount>1)memmove(gQueue,gQueue+1,sizeof(Event)*(gCount-1)); --gCount; clear(&gQueue[gCount],sizeof(Event));gInFlight=true;}
  xSemaphoreGive(gQueueMutex);return ok;
}
void finishEvent(bool success) {
  xSemaphoreTake(gQueueMutex,portMAX_DELAY);gInFlight=false;
  if(success)satInc(gSent);else satInc(gFailed);
  xSemaphoreGive(gQueueMutex);
}
bool getConfigSnapshot(Config& c, uint32_t& generation) {
  if(gConfigMutex)xSemaphoreTake(gConfigMutex,portMAX_DELAY);
  c=gConfigCache; generation=gConfigGeneration;
  if(gConfigMutex)xSemaphoreGive(gConfigMutex);
  return c.version==2&&c.enabled==1;
}
bool generationCurrent(uint32_t generation) {
  if(gConfigMutex)xSemaphoreTake(gConfigMutex,portMAX_DELAY);
  bool current=generation==gConfigGeneration&&gEnabled;
  if(gConfigMutex)xSemaphoreGive(gConfigMutex);
  return current;
}
void expireWaiting() {
  xSemaphoreTake(gQueueMutex,portMAX_DELAY);
  for(uint8_t i=0;i<gCount;){
    if(millis()-gQueue[i].enqueuedMs>=kTtlMs){
      if(gQueue[i].kind==EventKind::Test)gTestState=3;
      clear(&gQueue[i],sizeof(Event));if(i+1<gCount)memmove(gQueue+i,gQueue+i+1,sizeof(Event)*(gCount-i-1));--gCount;satInc(gFailed);
    }else ++i;
  }xSemaphoreGive(gQueueMutex);
}
void testOutcome(const Event& e,uint8_t outcome){if(e.kind==EventKind::Test){xSemaphoreTake(gQueueMutex,portMAX_DELAY);gTestState=outcome;xSemaphoreGive(gQueueMutex);}}
uint32_t quotaPeriod() {
  time_t stamp=TimeManager::epochSeconds()+9*3600;struct tm t;
  return gmtime_r(&stamp,&t) ? static_cast<uint32_t>((t.tm_year+1900)*12+t.tm_mon) : 0;
}
bool maybeQuota(Config& c,uint32_t generation) {
  HttpResult limit={},usage={};
  const bool a=lineRequest(c,"/v2/bot/message/quota",nullptr,nullptr,limit);
  if(!generationCurrent(generation))return false;
  if(a&&(limit.code==401||limit.code==403)){gAuthBlocked=true;updateState(State::AuthRequired);return false;}
  if(a&&limit.code==200&&!limit.quotaKnown){gAuthBlocked=true;updateState(State::AuthRequired);return false;}
  const bool b=a&&limit.code==200&&limit.quotaKnown&&lineRequest(c,"/v2/bot/message/quota/consumption",nullptr,nullptr,usage);
  if(!generationCurrent(generation))return false;
  if(b&&(usage.code==401||usage.code==403)){gAuthBlocked=true;updateState(State::AuthRequired);return false;}
  if(b&&usage.code==200&&usage.usageKnown){
    const uint32_t period=quotaPeriod();xSemaphoreTake(gConfigMutex,portMAX_DELAY);
    if(generation!=gConfigGeneration){xSemaphoreGive(gConfigMutex);return false;}
    // Approximate usage must not reduce local reservations within a month.
    if(!gQuotaKnown||period!=gQuotaPeriod)gQuotaUsed=usage.quotaUsed;
    else if(usage.quotaUsed>gQuotaUsed)gQuotaUsed=usage.quotaUsed;
    gQuotaPeriod=period;gQuotaKnown=true;gQuotaLimit=limit.quota;
    gQuotaFull=gQuotaUsed>=gQuotaLimit;
    gState=gQuotaFull?State::QuotaFull:State::Ready;xSemaphoreGive(gConfigMutex);return true;
  }
  updateState(State::ServiceError);return false;
}
// Broadcast body for every event kind: no "to" field, so LINE delivers the
// message to every friend of the Official Account.
bool buildDeliveryBody(const Event& e,int16_t timezoneMinutes,char text[1025],char* body,size_t size){
  if(!buildText(e,timezoneMinutes,text))return false;
  const int n=snprintf(body,size,"{\"messages\":[{\"type\":\"text\",\"text\":\"");
  size_t at=n>0?appendEscaped(body,size,static_cast<size_t>(n),text):SIZE_MAX;
  at=append(body,size,at,"\"}]}");
  return at<size;
}
void worker(void*) {
  uint32_t checkedGeneration=0,nextQuotaMs=0;
  for(;;){
    expireWaiting();Config cfg;uint32_t generation=0;
    if(!getConfigSnapshot(cfg,generation)){updateState(State::Disabled);clear(&cfg,sizeof(cfg));xSemaphoreTake(gWake,pdMS_TO_TICKS(1000));continue;}
    if(generation!=checkedGeneration){checkedGeneration=generation;nextQuotaMs=millis();}
    if(!gSta){updateState(State::Offline);clear(&cfg,sizeof(cfg));xSemaphoreTake(gWake,pdMS_TO_TICKS(1000));continue;}
    if(!TimeManager::synchronized()){updateState(State::TimeUnavailable);clear(&cfg,sizeof(cfg));xSemaphoreTake(gWake,pdMS_TO_TICKS(1000));continue;}
    if(gAuthBlocked){updateState(State::AuthRequired);clear(&cfg,sizeof(cfg));xSemaphoreTake(gWake,pdMS_TO_TICKS(1000));continue;}
    if(!heapAdmitted()){updateState(State::ServiceError);clear(&cfg,sizeof(cfg));xSemaphoreTake(gWake,pdMS_TO_TICKS(1000));continue;}
    if(static_cast<int32_t>(millis()-nextQuotaMs)>=0){
      const bool ok=maybeQuota(cfg,generation);
      nextQuotaMs=millis()+(ok?(gQuotaFull?21600000UL:900000UL):30000UL);
    }
    if(!generationCurrent(generation)||!gQuotaKnown||gQuotaFull||gAuthBlocked){clear(&cfg,sizeof(cfg));xSemaphoreTake(gWake,pdMS_TO_TICKS(1000));continue;}
    Event e;
    if(!takeEvent(e)){clear(&cfg,sizeof(cfg));xSemaphoreTake(gWake,pdMS_TO_TICKS(1000));continue;}
    bool sent=false,reserved=false,terminal=false,quotaRejected=false;uint8_t attempts=0;uint32_t retryAt=millis();
    char text[1025]={},body[2048]={};
    if(!buildDeliveryBody(e,cfg.timezoneMinutes,text,body,sizeof(body)))terminal=true;
    while(!terminal&&!sent&&attempts<kMaxAttempts&&millis()-e.enqueuedMs<kTtlMs&&generationCurrent(generation)){
      expireWaiting();
      if(!gSta||!TimeManager::synchronized()||gAuthBlocked||quotaRejected){xSemaphoreTake(gWake,pdMS_TO_TICKS(1000));continue;}
      if(static_cast<int32_t>(millis()-retryAt)<0){xSemaphoreTake(gWake,pdMS_TO_TICKS(1000));continue;}
      if(!heapAdmitted()){retryAt=millis()+1000;continue;}
      if(!reserved){xSemaphoreTake(gConfigMutex,portMAX_DELAY);if(gQuotaUsed<65535)++gQuotaUsed;gQuotaFull=gQuotaUsed>=gQuotaLimit;xSemaphoreGive(gConfigMutex);reserved=true;}
      ++attempts;HttpResult response;
      const bool complete=lineRequest(cfg,kDeliveryPath,body,e.retryKey,response);
      if(!generationCurrent(generation))break;
      if(complete&&(response.code==200||(response.code==409&&response.acceptedConflict)))sent=true;
      else if(complete&&(response.code==401||response.code==403)){gAuthBlocked=true;updateState(State::AuthRequired);}
      else if(complete&&response.code==429){
        const bool refreshed=maybeQuota(cfg,generation);
        if(!refreshed||gQuotaFull){quotaRejected=true;nextQuotaMs=millis()+21600000UL;updateState(State::QuotaFull);}
        else retryAt=millis()+30000;
      }else if(complete&&response.code>=400&&response.code<500)terminal=true;
      else updateState(State::ServiceError);
      if(!sent&&!terminal&&(!complete||response.code!=429)){
        static const uint32_t delays[]={5000,15000,30000,60000,120000,300000};
        retryAt=millis()+delays[attempts<6?attempts-1:5]+esp_random()%1000;
      }
    }
    testOutcome(e,sent?2:3);finishEvent(sent);
    if(sent)updateState(gQuotaFull?State::QuotaFull:State::Ready);
    // A broadcast counts once per friend; the +1 local reservation is only a lower
    // bound, so re-read official consumption shortly after any accepted send.
    if(sent){const uint32_t soon=millis()+5000;if(static_cast<int32_t>(nextQuotaMs-soon)>0)nextQuotaMs=soon;}
    clear(text,sizeof(text));clear(body,sizeof(body));clear(&e,sizeof(e));clear(&cfg,sizeof(cfg));
    xSemaphoreTake(gWake,pdMS_TO_TICKS(1000));
  }
}

void initLocks() {
  if(!gQueueMutex)gQueueMutex=xSemaphoreCreateMutex();
  if(!gConfigMutex)gConfigMutex=xSemaphoreCreateMutex();
  if(!gWake)gWake=xSemaphoreCreateBinary();
}
void fillBase(Event& e, EventKind kind) {
  memset(&e,0,sizeof(e)); e.kind=kind;e.enqueuedMs=millis();e.epoch=TimeManager::synchronized()?currentEpoch():0;
  if (gConfigMutex) xSemaphoreTake(gConfigMutex, portMAX_DELAY);
  strlcpy(e.label, gLabelCache[0] ? gLabelCache : kFallbackLabel, sizeof(e.label));
  if (gConfigMutex) xSemaphoreGive(gConfigMutex);
  makeUuid(e.retryKey);
}
} // namespace

void begin(bool staConnected) {
  initLocks();gSta=staConnected;refreshConfigState();
  if(!gQueueMutex||!gConfigMutex||!gWake){gEnabled=false;gState=State::ServiceError;return;}
  // TLS certificate verification needs more stack than the host transport fake.
  // Keep its worker isolated from the lock loop, including during a cold handshake.
  if(!gTask)xTaskCreatePinnedToCore(worker,"line-worker",24576,nullptr,1,&gTask,0);
  if(!gTask){gEnabled=false;gState=State::ServiceError;return;}
  if(gWake)xSemaphoreGive(gWake);
}
void update(bool staConnected) { const bool changed=gSta.exchange(staConnected)!=staConnected;if(changed&&gWake)xSemaphoreGive(gWake); }
Result save(const char* label,const char* token,const char* publicBasicId) {
  if(gSuspended)return Result::StorageError;
  if(!validLabel(label)||!validToken(token)||!validBasicId(publicBasicId))return Result::InvalidInput;
  Config c={};c.version=2;c.enabled=1;c.timezoneMinutes=420;
  strlcpy(c.label,(label&&label[0])?label:kFallbackLabel,sizeof(c.label));
  strlcpy(c.token,token,sizeof(c.token));strlcpy(c.publicBasicId,publicBasicId,sizeof(c.publicBasicId));
  bool ok=writeConfig(c);
  if(ok){
    if(gQueueMutex){xSemaphoreTake(gQueueMutex,portMAX_DELAY);for(uint8_t i=0;i<gCount;++i)clear(&gQueue[i],sizeof(Event));gCount=0;xSemaphoreGive(gQueueMutex);}
    refreshConfigState();if(gWake)xSemaphoreGive(gWake);
  }
  clear(&c,sizeof(c));
  return ok?Result::Ok:Result::StorageError;
}
Result rename(const char* label) {
  if(gSuspended)return Result::StorageError;
  if(!validLabel(label)||!label||!label[0])return Result::InvalidInput;
  Config c={};if(!readConfig(c))return Result::NotConfigured;
  c.version=2;strlcpy(c.label,label,sizeof(c.label));
  const bool ok=writeConfig(c);
  if(ok){
    if(gConfigMutex)xSemaphoreTake(gConfigMutex,portMAX_DELAY);
    gConfigCache=c;strlcpy(gLabelCache,c.label,sizeof(gLabelCache));gConfigVersion=2;
    if(gConfigMutex)xSemaphoreGive(gConfigMutex);
  }
  clear(&c,sizeof(c));return ok?Result::Ok:Result::StorageError;
}
Result disconnect() {
  if(gSuspended)return Result::StorageError;
  Config c={};c.version=2;c.enabled=0;c.timezoneMinutes=420;
  if(gConfigMutex)xSemaphoreTake(gConfigMutex,portMAX_DELAY);
  c.timezoneMinutes=gConfigCache.timezoneMinutes;
  strlcpy(c.label,gLabelCache[0]?gLabelCache:kFallbackLabel,sizeof(c.label));
  strlcpy(c.publicBasicId,gConfigCache.publicBasicId,sizeof(c.publicBasicId));
  if(gConfigMutex)xSemaphoreGive(gConfigMutex);
  bool ok=writeConfig(c);
  clear(&c,sizeof(c));
  if(ok){
    Preferences cleanup;
    if(cleanup.begin("sl-line",false)){
      const uint8_t activeNow=cleanup.getUChar("active",0xff);
      if(activeNow<=1)cleanup.remove(activeNow==0?"cfg1":"cfg0");
      cleanup.remove("config");
      cleanup.end();
    }
    if(gQueueMutex){xSemaphoreTake(gQueueMutex,portMAX_DELAY);for(uint8_t i=0;i<gCount;++i)clear(&gQueue[i],sizeof(Event));gCount=0;xSemaphoreGive(gQueueMutex);}
    refreshConfigState();if(gWake)xSemaphoreGive(gWake);
  }
  return ok?Result::Ok:Result::StorageError;
}
Result suspendForReset() {
  // Runtime-only stop for Factory Reset. The persisted sl-line configuration is
  // deliberately left untouched so it survives the reset (prototype policy).
  gSuspended=true;
  if(gConfigMutex)xSemaphoreTake(gConfigMutex,portMAX_DELAY);
  gEnabled=false;gConfigured=false;gState=State::Disabled;
  clear(&gConfigCache,sizeof(gConfigCache));++gConfigGeneration;
  if(gConfigMutex)xSemaphoreGive(gConfigMutex);
  if(gQueueMutex){xSemaphoreTake(gQueueMutex,portMAX_DELAY);for(uint8_t i=0;i<gCount;++i)clear(&gQueue[i],sizeof(Event));gCount=0;xSemaphoreGive(gQueueMutex);}
  if(gWake)xSemaphoreGive(gWake);
  return Result::Ok;
}
void status(Status& out) {
  memset(&out,0,sizeof(out));
  if(gConfigMutex)xSemaphoreTake(gConfigMutex,portMAX_DELAY);
  out.configured=gConfigured;out.enabled=gEnabled;out.staConnected=gSta;out.quotaKnown=gQuotaKnown;
  out.configVersion=gConfigVersion;
  out.quotaLimit=gQuotaLimit;out.quotaUsed=gQuotaUsed;out.state=gState;
  if(gConfigMutex)xSemaphoreGive(gConfigMutex);
  if(gQueueMutex){xSemaphoreTake(gQueueMutex,portMAX_DELAY);out.queued=gCount+(gInFlight?1:0);out.sentThisRun=gSent;out.failedThisRun=gFailed;out.testState=gTestState;xSemaphoreGive(gQueueMutex);}
  if(gConfigMutex)xSemaphoreTake(gConfigMutex,portMAX_DELAY);
  strlcpy(out.deviceLabel,gLabelCache[0]?gLabelCache:kFallbackLabel,sizeof(out.deviceLabel));
  strlcpy(out.publicBasicId,gConfigCache.publicBasicId,sizeof(out.publicBasicId));
  if(gConfigMutex)xSemaphoreGive(gConfigMutex);
}
void telemetry(Telemetry& out){
 out={gUnlockGenerated,gPinGenerated,gEmergencyGenerated,gEnqueued,gPushStarted,gTlsConnected,gPushWritten,gResponses,gHeapBlocked,gAdmissionFree,gAdmissionLargest,gAdmissionMinimum,gTlsMinimum,gTlsAfterLargest,gLastPushHttp};
}
Result requestTest() {
  if(!gEnabled)return Result::NotConfigured;
  const uint32_t now=millis();
  if(gTestStarted&&now-gLastTestMs<kTestIntervalMs)return Result::RateLimited;
  gTestStarted=true;gLastTestMs=now;
  xSemaphoreTake(gQueueMutex,portMAX_DELAY);gTestState=1;xSemaphoreGive(gQueueMutex);
  Event e;fillBase(e,EventKind::Test);enqueue(e);return Result::Ok;
}
void unlockSuccess(const char* id,const char* name,const char* role,const char* source) {
  ++gUnlockGenerated;
  if(!gEnabled)return;
  Event e;fillBase(e,EventKind::Unlock);
  const size_t ni=boundedLen(id,32),nn=boundedLen(name,64),nr=boundedLen(role,23),ns=boundedLen(source,23);
  if(ni>32||nn>64||nr>23||ns>23||!validUtf8(name,nn)){clear(&e,sizeof(e));return;}
  strlcpy(e.userId,id?id:"",sizeof(e.userId));strlcpy(e.identityName,name?name:"",sizeof(e.identityName));
  strlcpy(e.role,role?role:"",sizeof(e.role));strlcpy(e.source,source?source:"",sizeof(e.source));enqueue(e);
}
void pinFailed(uint8_t count,bool locked,bool lockout) {
  ++gPinGenerated;
  if(!gEnabled)return;Event e;fillBase(e,EventKind::PinFailure);e.wrongCount=count;e.locked=locked;e.lockout=lockout;enqueue(e);
}
void emergencyUnlock() { ++gEmergencyGenerated; if(!gEnabled)return;Event e;fillBase(e,EventKind::Emergency);enqueue(e); }

} // namespace LineNotifications
