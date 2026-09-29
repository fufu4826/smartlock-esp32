#pragma once
#include <cstddef>
void bootloader_random_enable();
void bootloader_random_disable();
void esp_fill_random(void* output,size_t length);
