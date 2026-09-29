#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
using esp_err_t = int;
using esp_ota_handle_t = uint32_t;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_FAIL = -1;
struct esp_partition_t { uint32_t address; std::size_t size; };
namespace ota_test {
extern bool failOtaBegin, failOtaWrite, failOtaEnd, failSetBoot;
extern uint32_t runningAddress, bootAddress;
extern std::string trace;
extern std::string writtenImage;
}
const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t*);
const esp_partition_t* esp_ota_get_running_partition();
esp_err_t esp_ota_begin(const esp_partition_t*, std::size_t, esp_ota_handle_t*);
esp_err_t esp_ota_write(esp_ota_handle_t, const void*, std::size_t);
esp_err_t esp_ota_end(esp_ota_handle_t);
esp_err_t esp_ota_abort(esp_ota_handle_t);
esp_err_t esp_ota_set_boot_partition(const esp_partition_t*);
esp_err_t esp_partition_read(const esp_partition_t*, std::size_t, void*, std::size_t);
