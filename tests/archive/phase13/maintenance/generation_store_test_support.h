#pragma once
#include <stddef.h>
#include <stdint.h>
#include "ConfigStore.h"
#include "GenerationStore.h"
#include "SecureBackup.h"
#include "AtomicFileStore.h"
#include "SD.h"

extern SDClass SD;
uint32_t millis();
void resetGenerationFakes();
void seedLegacyFiles();
void makeCurrentSnapshot(const SmartLockConfig& config, SecureBackup::Buffer& output,
                         const char* ssid, const char* apPassword);
bool verifySelectedGeneration(const SmartLockConfig& expectedConfig,
                              const char* expectedSsid, const char* expectedAp);
bool verifyLegacyGeneration(const SmartLockConfig& expectedConfig,
                            const char* expectedSsid, const char* expectedAp);
void setCaptureNetwork(const char* ssid, const char* apPassword);
