#pragma once
#include <stddef.h>
#include <stdint.h>
inline void esp_fill_random(void* output, size_t length) { if (output) for (size_t i=0;i<length;++i) static_cast<uint8_t*>(output)[i]=0; }
