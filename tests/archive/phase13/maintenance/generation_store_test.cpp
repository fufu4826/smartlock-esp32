#include "generation_store_test_support.h"
#include "Preferences.h"
#include "MaintenanceBarrier.h"
#include <cstdio>
#include <cstring>

static int assertions=0;
#define CHECK(x) do {++assertions;if(!(x)){std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);return false;}} while(0)
static uint32_t configCrc(const SmartLockConfig& c){const uint8_t* p=(const uint8_t*)&c;uint32_t v=0xffffffffu;for(size_t i=0;i<offsetof(SmartLockConfig,crc32);++i){v^=p[i];for(unsigned b=0;b<8;++b)v=(v>>1)^(0xedb88320u&(0u-(v&1u)));}return ~v;}
static SmartLockConfig candidateConfig(const SmartLockConfig& base){SmartLockConfig c=base;std::strcpy(c.deviceName,"Restored Lock");c.configured=1;c.ownerExists=1;c.networkMode=2;c.generation=base.generation+4;c.crc32=configCrc(c);return c;}
static bool startLegacy(ConfigStore& config){seedLegacyFiles();if(!GenerationStore::begin()||GenerationStore::active())return false;auto r=config.begin();if((r!=ConfigStore::Result::CreatedDefaults&&r!=ConfigStore::Result::Ok)||!config.healthy())return false;SmartLockConfig owner=config.config();owner.configured=1;owner.ownerExists=1;std::strcpy(owner.deviceName,"Factory Owner");return config.save(owner);}
static bool matchesOldOrCandidate(const ConfigStore& recovered,const SmartLockConfig& oldConfig,const SmartLockConfig& candidate){
  const SmartLockConfig& actual=recovered.config();
  if(!std::memcmp(&actual,&candidate,sizeof(actual)))return GenerationStore::active()&&verifySelectedGeneration(candidate,"restored-net","abcdef0123456789abcdef0123456789");
  if(std::memcmp(&actual,&oldConfig,sizeof(actual)))return false;
  if(GenerationStore::active())return verifySelectedGeneration(oldConfig,"factory-net","0123456789abcdef0123456789abcdef");
  return verifyLegacyGeneration(oldConfig,"factory-net","0123456789abcdef0123456789abcdef");
}
static bool activateCandidate(ConfigStore& config,const SmartLockConfig& candidate,const char* ssid="restored-net",const char* ap="abcdef0123456789abcdef0123456789"){
  SecureBackup::Buffer package;makeCurrentSnapshot(candidate,package,ssid,ap);
  if(!MaintenanceBarrier::acquire(MaintenanceBarrier::Operation::Restore))return false;
  return GenerationStore::activate(package.bytes.get(),package.size,config);
}
static bool testSuccessfulActivateFinalizeAndSubsequentChanges(){
  resetGenerationFakes();ConfigStore config;CHECK(startLegacy(config));auto candidate=candidateConfig(config.config());
  CHECK(activateCandidate(config,candidate));CHECK(GenerationStore::rebootRequired());CHECK(MaintenanceBarrier::busy());
  MaintenanceBarrier::release();CHECK(GenerationStore::begin());ConfigStore boot;CHECK(GenerationStore::ready());CHECK(GenerationStore::active());CHECK(std::strcmp(GenerationStore::recoveryState(),"recovered_pending_initialization")==0);CHECK(MaintenanceBarrier::busy());
  CHECK(!GenerationStore::finalizeBoot(false));CHECK(!boot.healthy());CHECK(GenerationStore::finalizeBoot(true));CHECK(!MaintenanceBarrier::busy());
  CHECK(boot.begin()==ConfigStore::Result::Ok);CHECK(verifySelectedGeneration(candidate,"restored-net","abcdef0123456789abcdef0123456789"));
  SmartLockConfig updated=boot.config();std::strcpy(updated.deviceName,"After finalization");CHECK(boot.save(updated));
  CHECK(GenerationStore::saveSta("changed-net","changed-password"));CHECK(GenerationStore::saveAp("0123456789abcdef0123456789abcdef"));
  const uint16_t calibration[5]={9,8,7,6,5};CHECK(GenerationStore::saveCalibration(calibration));
  const char authChanged[]="D000001|new-verifier\n";CHECK(AtomicFileStore::write(GenerationStore::authPath(),(const uint8_t*)authChanged,sizeof(authChanged)-1));
  CHECK(GenerationStore::begin());ConfigStore rebooted;CHECK(rebooted.begin()==ConfigStore::Result::Ok);CHECK(std::strcmp(rebooted.config().deviceName,"After finalization")==0);
  char ap[33]={},ssid[33]={},pass[65]={};bool sta=false;CHECK(GenerationStore::loadNetwork(ap,ssid,pass,sta));CHECK(std::strcmp(ssid,"changed-net")==0&&std::strcmp(pass,"changed-password")==0&&std::strcmp(ap,"0123456789abcdef0123456789abcdef")==0);
  uint16_t loaded[5]={};bool cal=false;CHECK(GenerationStore::loadCalibration(loaded,cal));CHECK(cal&&loaded[0]==9&&loaded[4]==5);
  uint8_t authData[64];size_t authLength=0;CHECK(AtomicFileStore::read(GenerationStore::authPath(),authData,sizeof(authData),authLength)==AtomicFileStore::ReadResult::CurrentValid);CHECK(authLength==sizeof(authChanged)-1&&!std::memcmp(authData,authChanged,authLength));
  CHECK(std::strcmp(GenerationStore::recoveryState(),"active_generation")==0);return true;
}
static bool testRejectInvalidCandidateWithoutChangingLegacy(){
  resetGenerationFakes();ConfigStore config;CHECK(startLegacy(config));auto candidate=candidateConfig(config.config());SecureBackup::Buffer package;makeCurrentSnapshot(candidate,package,"restore","abcdef0123456789abcdef0123456789");
  CHECK(MaintenanceBarrier::acquire(MaintenanceBarrier::Operation::Restore));package.bytes.get()[package.size-1]^=1;CHECK(!GenerationStore::activate(package.bytes.get(),package.size,config));MaintenanceBarrier::release();
  CHECK(GenerationStore::begin());CHECK(verifyLegacyGeneration(config.config(),"factory-net","0123456789abcdef0123456789abcdef"));
  makeCurrentSnapshot(candidate,package,"restore","abcdef0123456789abcdef0123456789");reinterpret_cast<SecureBackup::Snapshot*>(package.bytes.get())->board^=1;
  CHECK(MaintenanceBarrier::acquire(MaintenanceBarrier::Operation::Restore));CHECK(!GenerationStore::activate(package.bytes.get(),package.size,config));MaintenanceBarrier::release();CHECK(GenerationStore::begin());CHECK(!GenerationStore::active());return true;
}
static bool testNvsCommitFaults(){
  // Each injected failure models a reset boundary before or after a durable
  // putBytes commit. Puts 0..5 are old bundle, candidate bundle, journal
  // Prepared, journal Started, selector, and journal Activated.
  int faultCases=0;
  for(size_t op=0;op<6;++op)for(unsigned after=0;after<2;++after){
    resetGenerationFakes();ConfigStore config;CHECK(startLegacy(config));const SmartLockConfig oldConfig=config.config();auto candidate=candidateConfig(oldConfig);FakePreferences::failPutAt(op,after!=0);
    (void)activateCandidate(config,candidate);FakePreferences::disableFault();MaintenanceBarrier::release();CHECK(GenerationStore::begin());
    if(MaintenanceBarrier::busy()){CHECK(!GenerationStore::finalizeBoot(false));CHECK(GenerationStore::finalizeBoot(true));CHECK(!MaintenanceBarrier::busy());}
    CHECK(GenerationStore::ready());ConfigStore recovered;auto cr=recovered.begin();CHECK(cr==ConfigStore::Result::Ok||cr==ConfigStore::Result::CreatedDefaults);
    CHECK(matchesOldOrCandidate(recovered,oldConfig,candidate));++faultCases;
  }
  // Finalized is the authority boundary. A put failure before commit retries
  // initialization on reboot; a failure after commit observes Finalized.
  for(unsigned after=0;after<2;++after){resetGenerationFakes();ConfigStore config;CHECK(startLegacy(config));auto candidate=candidateConfig(config.config());CHECK(activateCandidate(config,candidate));MaintenanceBarrier::release();CHECK(GenerationStore::begin());FakePreferences::failPutAt(0,after!=0);CHECK(!GenerationStore::finalizeBoot(true));FakePreferences::disableFault();MaintenanceBarrier::release();CHECK(GenerationStore::begin());if(MaintenanceBarrier::busy())CHECK(GenerationStore::finalizeBoot(true));ConfigStore recovered;CHECK(recovered.begin()==ConfigStore::Result::Ok);CHECK(verifySelectedGeneration(candidate,"restored-net","abcdef0123456789abcdef0123456789"));++faultCases;}
  std::printf("  NVS durable fault cases: %d commit boundary injections\n",faultCases);
  return true;
}
static bool testSdMutationFaults(){
  // Fail each attempted SD mutation once. A fresh fake volume is used per case;
  // the preceding successful writes remain durable through the simulated reboot.
  size_t cases=0;bool reachedCompletion=false;
  for(size_t boundary=0;boundary<30;++boundary){
    resetGenerationFakes();ConfigStore config;CHECK(startLegacy(config));const SmartLockConfig oldConfig=config.config();auto candidate=candidateConfig(oldConfig);SD.failAfterMutations(boundary);
    bool activated=activateCandidate(config,candidate);bool fault=SD.faultTriggered();
    if(!fault){CHECK(activated);reachedCompletion=true;break;}
    MaintenanceBarrier::release();CHECK(GenerationStore::begin());
    if(MaintenanceBarrier::busy()){CHECK(!GenerationStore::finalizeBoot(false));CHECK(GenerationStore::finalizeBoot(true));}
    CHECK(GenerationStore::ready());
    ConfigStore recovered;auto cr=recovered.begin();CHECK(cr==ConfigStore::Result::Ok||cr==ConfigStore::Result::CreatedDefaults);
    CHECK(matchesOldOrCandidate(recovered,oldConfig,candidate));++cases;
  }
  CHECK(reachedCompletion);std::printf("  SD fake fail-before mutation boundaries: %zu\n",cases);
  return true;
}
static bool testSelectorAndJournalCorruptionFailClosed(){
  resetGenerationFakes();ConfigStore config;CHECK(startLegacy(config));auto candidate=candidateConfig(config.config());CHECK(activateCandidate(config,candidate));
  CHECK(FakePreferences::corrupt("active",0));MaintenanceBarrier::release();CHECK(!GenerationStore::begin());CHECK(!GenerationStore::ready());CHECK(std::strcmp(GenerationStore::recoveryState(),"blocked")==0);CHECK(MaintenanceBarrier::busy());
  ConfigStore denied;CHECK(denied.begin()==ConfigStore::Result::Corrupt);
  resetGenerationFakes();ConfigStore second;CHECK(startLegacy(second));candidate=candidateConfig(second.config());CHECK(activateCandidate(second,candidate));
  CHECK(FakePreferences::corrupt("txn",0));MaintenanceBarrier::release();CHECK(!GenerationStore::begin());CHECK(!GenerationStore::ready());CHECK(MaintenanceBarrier::busy());
  ConfigStore deniedJournal;CHECK(deniedJournal.begin()==ConfigStore::Result::Corrupt);return true;
}
static bool testCorruptCandidateRollsBackOnlyToStagedAnchor(){
  // Deterministic fake entropy yields old generation 0x75 and candidate 0x86.
  resetGenerationFakes();ConfigStore config;CHECK(startLegacy(config));const SmartLockConfig oldConfig=config.config();auto candidate=candidateConfig(oldConfig);CHECK(activateCandidate(config,candidate));
  CHECK(FakePreferences::corrupt("g00000086",0));MaintenanceBarrier::release();CHECK(GenerationStore::begin());CHECK(MaintenanceBarrier::busy());CHECK(std::strcmp(GenerationStore::recoveryState(),"recovered_pending_initialization")==0);
  CHECK(GenerationStore::finalizeBoot(true));ConfigStore recovered;CHECK(recovered.begin()==ConfigStore::Result::Ok);CHECK(std::memcmp(&recovered.config(),&oldConfig,sizeof(oldConfig))==0);CHECK(GenerationStore::active());CHECK(verifySelectedGeneration(oldConfig,"factory-net","0123456789abcdef0123456789abcdef"));

  resetGenerationFakes();ConfigStore second;CHECK(startLegacy(second));auto secondCandidate=candidateConfig(second.config());CHECK(activateCandidate(second,secondCandidate));
  CHECK(FakePreferences::corrupt("g00000075",0));CHECK(FakePreferences::corrupt("g00000086",0));MaintenanceBarrier::release();CHECK(!GenerationStore::begin());CHECK(!GenerationStore::ready());CHECK(MaintenanceBarrier::busy());ConfigStore denied;CHECK(denied.begin()==ConfigStore::Result::Corrupt);return true;
}
int main(){
  if(!testSuccessfulActivateFinalizeAndSubsequentChanges()||!testRejectInvalidCandidateWithoutChangingLegacy()||!testNvsCommitFaults()||!testSdMutationFaults()||!testSelectorAndJournalCorruptionFailClosed()||!testCorruptCandidateRollsBackOnlyToStagedAnchor())return 1;
  std::printf("GenerationStore host integration: %d assertions passed\n",assertions);return 0;
}
