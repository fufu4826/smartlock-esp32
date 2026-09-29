#pragma once
#include <stddef.h>
#include <stdint.h>
constexpr uint32_t MALLOC_CAP_8BIT=1;
inline size_t heap_caps_get_free_size(uint32_t){return 200000;}
inline size_t fakeLargestBlock=100000;
inline size_t heap_caps_get_largest_free_block(uint32_t){return fakeLargestBlock;}
