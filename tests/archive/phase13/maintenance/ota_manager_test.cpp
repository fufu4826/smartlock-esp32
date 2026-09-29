#include "OtaManager.h"
#include "MaintenanceBarrier.h"
#include "ota_manager_mocks/Arduino.h"
#include "ota_manager_mocks/ConfigStore.h"
#include "ota_manager_mocks/EnrollmentManager.h"
#include "ota_manager_mocks/Preferences.h"
#include "ota_manager_mocks/SecureBackup.h"
#include "ota_manager_mocks/esp_ota_ops.h"
#include "ota_manager_mocks/esp_system.h"
#include "ota_manager_mocks/mbedtls/sha256.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

uint32_t hostMillis = 5000;
FakeSerial Serial;
namespace ota_test {
std::string nvsPath;
bool failPreferenceWrite = false;
bool failOtaBegin = false, failOtaWrite = false, failOtaEnd = false, failSetBoot = false;
uint32_t runningAddress = 0x10000, bootAddress = 0x10000;
std::string trace;
std::string writtenImage;
uint8_t snapshotMarker = 7;
unsigned randomCounter = 0;
bool loadNvs(std::string& out) {
  std::ifstream input(nvsPath, std::ios::binary);
  if (!input) return false;
  out.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
  return bool(input) || input.eof();
}
bool saveNvs(const std::string& value) {
  std::ofstream output(nvsPath, std::ios::binary | std::ios::trunc);
  output.write(value.data(), static_cast<std::streamsize>(value.size()));
  return bool(output);
}
}

static unsigned checkCount = 0;
#undef assert
#define assert(condition) do { \
  ++checkCount; \
  if (!(condition)) { \
    std::cerr << "CHECK failed at line " << __LINE__ << ": " #condition "\n"; \
    std::abort(); \
  } \
} while (0)

static const esp_partition_t updatePartition{0x20000, 4096};
static esp_partition_t runningPartition{0x10000, 4096};
const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t*) {
  return &updatePartition;
}
const esp_partition_t* esp_ota_get_running_partition() {
  runningPartition.address = ota_test::runningAddress;
  return &runningPartition;
}
esp_err_t esp_ota_begin(const esp_partition_t* part, std::size_t, esp_ota_handle_t* handle) {
  ota_test::trace += "begin;";
  if (ota_test::failOtaBegin || part != &updatePartition) return ESP_FAIL;
  *handle = 1;
  return ESP_OK;
}
esp_err_t esp_ota_write(esp_ota_handle_t, const void* data, std::size_t size) {
  ota_test::trace += "write;";
  if (ota_test::failOtaWrite) return ESP_FAIL;
  ota_test::writtenImage.append(static_cast<const char*>(data), size);
  std::ofstream partition(ota_test::nvsPath + ".partition", std::ios::binary | std::ios::trunc);
  partition.write(ota_test::writtenImage.data(), static_cast<std::streamsize>(ota_test::writtenImage.size()));
  if (!partition) return ESP_FAIL;
  return ESP_OK;
}
esp_err_t esp_ota_end(esp_ota_handle_t) {
  ota_test::trace += "end;";
  return ota_test::failOtaEnd ? ESP_FAIL : ESP_OK;
}
esp_err_t esp_ota_abort(esp_ota_handle_t) {
  ota_test::trace += "abort;";
  return ESP_OK;
}
esp_err_t esp_ota_set_boot_partition(const esp_partition_t* part) {
  ota_test::trace += "set-boot;";
  if (ota_test::failSetBoot || part != &updatePartition) return ESP_FAIL;
  ota_test::bootAddress = part->address;
  return ESP_OK;
}
esp_err_t esp_partition_read(const esp_partition_t* part, std::size_t offset, void* output, std::size_t size) {
  if (!part || part->address != ota_test::runningAddress) return ESP_FAIL;
  std::ifstream partition(ota_test::nvsPath + ".partition", std::ios::binary);
  if (!partition) return ESP_FAIL;
  partition.seekg(static_cast<std::streamoff>(offset));
  partition.read(static_cast<char*>(output), static_cast<std::streamsize>(size));
  return partition.gcount() == static_cast<std::streamsize>(size) ? ESP_OK : ESP_FAIL;
}
void esp_fill_random(void* output, std::size_t size) {
  auto* bytes = static_cast<uint8_t*>(output);
  for (std::size_t i = 0; i < size; ++i) bytes[i] = static_cast<uint8_t>(ota_test::randomCounter++);
}

static void digestFeed(mbedtls_sha256_context* context, const unsigned char* input, std::size_t size) {
  for (std::size_t i = 0; i < size; ++i) {
    context->state ^= input[i];
    context->state *= 1099511628211ull;
    ++context->length;
  }
}
static void digestOutput(const mbedtls_sha256_context* context, unsigned char output[32]) {
  uint64_t state = context->state ^ (context->length * 0x9e3779b97f4a7c15ull);
  for (unsigned i = 0; i < 32; ++i) {
    state ^= state >> 12; state ^= state << 25; state ^= state >> 27;
    output[i] = static_cast<unsigned char>((state * 2685821657736338717ull) >> 56);
  }
}
void mbedtls_sha256_init(mbedtls_sha256_context* context) { context->state = 1469598103934665603ull; context->length = 0; }
void mbedtls_sha256_free(mbedtls_sha256_context*) {}
int mbedtls_sha256_starts_ret(mbedtls_sha256_context* context, int) { mbedtls_sha256_init(context); return 0; }
int mbedtls_sha256_update_ret(mbedtls_sha256_context* context, const unsigned char* input, std::size_t size) {
  digestFeed(context, input, size); return 0;
}
int mbedtls_sha256_finish_ret(mbedtls_sha256_context* context, unsigned char output[32]) {
  digestOutput(context, output); return 0;
}
int mbedtls_sha256_ret(const unsigned char* input, std::size_t size, unsigned char output[32], int) {
  mbedtls_sha256_context context{}; mbedtls_sha256_init(&context); digestFeed(&context, input, size); digestOutput(&context, output); return 0;
}

static std::vector<uint8_t> image() {
  std::vector<uint8_t> data(1030, 0x5a);
  data[0] = 0xe9; data[1] = 1; data[2] = 0;
  data[12] = 0; data[13] = 0;
  return data;
}

static std::string expectedProof() {
  uint8_t ignored[32], raw[32];
  esp_fill_random(ignored, sizeof(ignored));
  esp_fill_random(raw, sizeof(raw));
  char result[65]{};
  for (unsigned i = 0; i < 32; ++i) std::snprintf(result + 2 * i, 3, "%02x", raw[i]);
  return result;
}

static bool startUpload(EnrollmentManager& enrollment, ConfigStore& config,
                        char (&ticket)[65], char (&proof)[65]) {
  return OtaManager::start(image().size(), std::string(64, 'a').c_str(), &enrollment, config, ticket, proof);
}

static void uploadAll(const char* ticket, const std::vector<uint8_t>& data) {
  assert(OtaManager::chunk(ticket, 0, data.data(), 1024));
  assert(OtaManager::accepted() == 1024);
  assert(OtaManager::chunk(ticket, 1024, data.data() + 1024, data.size() - 1024));
  assert(OtaManager::accepted() == data.size());
}

static void guardScenario(const std::string& which) {
  EnrollmentManager enrollment;
  ConfigStore config;
  char ticket[65]{}, proof[65]{};
  if (which == "expired-start") enrollment.expiresAt = hostMillis - 1;
  if (which == "revoked-start") enrollment.allow = false;
  if (which == "expired-start" || which == "revoked-start") {
    assert(!startUpload(enrollment, config, ticket, proof));
    assert(!OtaManager::busy() && ota_test::bootAddress == 0x10000);
    return;
  }
  assert(startUpload(enrollment, config, ticket, proof));
  assert(OtaManager::busy());
  if (which == "expired-chunk") {
    const auto data = image();
    enrollment.expiresAt = hostMillis - 1;
    assert(!OtaManager::chunk(ticket, 0, data.data(), 1024));
    assert(!OtaManager::busy() && ota_test::bootAddress == 0x10000);
    return;
  }
  if (which == "revoked-finish") {
    uploadAll(ticket, image());
    enrollment.allow = false;
    char sha[65]{};
    assert(!OtaManager::finish(ticket, sha));
    assert(!OtaManager::busy() && ota_test::bootAddress == 0x10000);
    return;
  }
  if (which == "expired-during") enrollment.expiresAt = hostMillis - 1;
  else enrollment.allow = false;
  OtaManager::update();
  assert(!OtaManager::busy());
  assert(ota_test::trace.find("abort;") != std::string::npos);
  assert(ota_test::bootAddress == 0x10000);
}

static void invalidTicketsDoNotAbort() {
  EnrollmentManager enrollment;
  ConfigStore config;
  char ticket[65]{}, proof[65]{};
  assert(startUpload(enrollment, config, ticket, proof));
  const std::string wrong(64, 'f');
  const auto data = image();
  assert(!OtaManager::chunk(wrong.c_str(), 0, data.data(), 1024));
  assert(!OtaManager::finish(wrong.c_str(), proof));
  assert(OtaManager::busy() && OtaManager::accepted() == 0);
  uploadAll(ticket, data);
  OtaManager::abort();
  assert(!OtaManager::busy() && ota_test::bootAddress == 0x10000);
}

static void failureScenario(const std::string& which) {
  EnrollmentManager enrollment;
  ConfigStore config;
  char ticket[65]{}, proof[65]{}, sha[65]{};
  if (which == "begin-failure") ota_test::failOtaBegin = true;
  if (which == "write-failure") ota_test::failOtaWrite = true;
  const bool started = startUpload(enrollment, config, ticket, proof);
  if (which == "begin-failure") {
    assert(!started && !OtaManager::busy());
    assert(ota_test::bootAddress == 0x10000 && ota_test::trace.find("set-boot;") == std::string::npos);
    return;
  }
  assert(started);
  const auto data = image();
  if (which == "write-failure") {
    assert(!OtaManager::chunk(ticket, 0, data.data(), 1024));
  } else {
    assert(OtaManager::chunk(ticket, 0, data.data(), 1024));
    assert(OtaManager::chunk(ticket, 1024, data.data() + 1024, data.size() - 1024));
    if (which == "end-failure") ota_test::failOtaEnd = true;
    if (which == "receipt-failure") ota_test::failPreferenceWrite = true;
    if (which == "setboot-failure") ota_test::failSetBoot = true;
    assert(!OtaManager::finish(ticket, sha));
  }
  assert(!OtaManager::busy());
  assert(ota_test::bootAddress == 0x10000);
  assert(ota_test::trace.find("set-boot;") == std::string::npos || which == "setboot-failure");
}

static std::string getNvs() {
  std::string bytes;
  assert(ota_test::loadNvs(bytes));
  return bytes;
}

static void successfulUpload() {
  EnrollmentManager enrollment;
  ConfigStore config;
  char ticket[65]{}, proof[65]{}, sha[65]{};
  const auto data = image();
  assert(startUpload(enrollment, config, ticket, proof));
  uploadAll(ticket, data);
  assert(OtaManager::finish(ticket, sha));
  assert(OtaManager::busy() && OtaManager::accepted() == data.size());
  assert(!OtaManager::rebootDue(6499) && OtaManager::rebootDue(6500));
  assert(ota_test::bootAddress == 0x20000);
  assert(ota_test::trace.find("end;receipt;set-boot;") != std::string::npos);
  const std::string stored = getNvs();
  assert(stored.size() == 120); // fixed receipt: hashes and metadata, never image bytes
  assert(stored.size() < data.size());
  assert(ota_test::writtenImage.size() == data.size());
  assert(ota_test::writtenImage.compare(0, data.size(), reinterpret_cast<const char*>(data.data()), data.size()) == 0);
  assert(ota_test::trace.find("write;write;") != std::string::npos);
  assert(stored.find(proof) == std::string::npos && stored.find(ticket) == std::string::npos);
  assert(OtaManager::result(proof) == std::string("pending"));
  assert(OtaManager::result(std::string(64, '0').c_str()) == std::string("invalid"));
}

static void bootCheckScenario(const std::string& scenario) {
  ota_test::runningAddress = 0x20000;
  ota_test::bootAddress = 0x20000;
  const bool changedSnapshot = scenario == "boot-changed";
  const bool changedImage = scenario == "boot-image-changed";
  const bool offlineTimeout = scenario == "boot-offline";
  ota_test::snapshotMarker = changedSnapshot ? 8 : 7;
  ConfigStore config;
  const std::string proof = expectedProof();
  std::string stored = getNvs();
  assert(stored.size() == 120 && static_cast<uint8_t>(stored[8]) == 1);
  if (changedImage) {
    std::fstream partition(ota_test::nvsPath + ".partition", std::ios::binary | std::ios::in | std::ios::out);
    assert(bool(partition));
    partition.seekp(25);
    const char corrupted = 0x33;
    partition.write(&corrupted, 1);
    assert(bool(partition));
  }
  OtaManager::bootBegin();
  assert(MaintenanceBarrier::operation() == MaintenanceBarrier::Operation::Ota);
  assert(MaintenanceBarrier::busy());
  if (offlineTimeout) hostMillis = 119999;
  OtaManager::bootCheck(config, true, false);
  assert(OtaManager::result(proof.c_str()) == std::string("pending"));
  stored = getNvs();
  assert(static_cast<uint8_t>(stored[8]) == 1); // disconnected STA cannot publish a result
  if (offlineTimeout) {
    assert(MaintenanceBarrier::busy());
    hostMillis = 120000;
    OtaManager::bootCheck(config, true, false);
    assert(OtaManager::result(proof.c_str()) == std::string("failed"));
    assert(!MaintenanceBarrier::busy());
    assert(static_cast<uint8_t>(getNvs()[8]) == 3);
    return;
  }
  OtaManager::bootCheck(config, true, true);
  const bool shouldFail = changedSnapshot || changedImage;
  assert(OtaManager::result(proof.c_str()) == std::string(shouldFail ? "failed" : "completed"));
  stored = getNvs();
  assert(static_cast<uint8_t>(stored[8]) == static_cast<uint8_t>(shouldFail ? 3 : 2));
  assert(!MaintenanceBarrier::busy());
}

int main(int argc, char** argv) {
  if (argc != 3) { std::cerr << "usage: ota_manager_tests <scenario> <nvs-file>\n"; return 2; }
  ota_test::nvsPath = argv[2];
  const std::string scenario = argv[1];
  if (scenario == "end-failure") ota_test::failOtaEnd = true;
  if (scenario == "receipt-failure") ota_test::failPreferenceWrite = true;
  if (scenario == "setboot-failure") ota_test::failSetBoot = true;
  if (scenario == "boot" || scenario == "boot-changed") ota_test::runningAddress = 0x20000;
  if (scenario == "expired-start" || scenario == "revoked-start" || scenario == "expired-chunk" ||
      scenario == "revoked-finish" || scenario == "expired-during" || scenario == "revoked-during") guardScenario(scenario);
  else if (scenario == "invalid-ticket") invalidTicketsDoNotAbort();
  else if (scenario == "success") successfulUpload();
  else if (scenario == "begin-failure" || scenario == "write-failure" || scenario == "end-failure" ||
           scenario == "receipt-failure" || scenario == "setboot-failure") failureScenario(scenario);
  else if (scenario == "boot" || scenario == "boot-changed" || scenario == "boot-image-changed" ||
           scenario == "boot-offline") bootCheckScenario(scenario);
  else { std::cerr << "unknown scenario: " << scenario << "\n"; return 2; }
  std::cout << "PASS: OtaManager " << scenario << " (" << checkCount << " checks)\n";
  return 0;
}
