#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
using std::size_t;
uint32_t fakeMillis();
void fakeAdvance(uint32_t ms);
inline uint32_t millis() { return fakeMillis(); }
inline size_t strlcpy(char* d,const char* s,size_t n){size_t z=strlen(s);if(n){size_t c=z<n-1?z:n-1;memcpy(d,s,c);d[c]=0;}return z;}
typedef void* SemaphoreHandle_t;
typedef void* TaskHandle_t;
typedef void (*TaskFunction_t)(void*);
inline uint32_t fakeFreeHeap=200000, fakeMinimumHeap=180000;
struct FakeESP { uint32_t getFreeHeap() const { return fakeFreeHeap; } uint32_t getMinFreeHeap() const { return fakeMinimumHeap; } };
extern FakeESP ESP;
constexpr uint32_t portMAX_DELAY=0xffffffffu;
#define pdMS_TO_TICKS(x) (x)
SemaphoreHandle_t xSemaphoreCreateMutex();
SemaphoreHandle_t xSemaphoreCreateBinary();
int xSemaphoreTake(SemaphoreHandle_t,uint32_t);
int xSemaphoreGive(SemaphoreHandle_t);
int xTaskCreatePinnedToCore(TaskFunction_t,const char*,uint32_t,void*,uint32_t,TaskHandle_t*,int);
void vTaskDelay(uint32_t);
