#pragma once

#include <Arduino.h>
#include <SPI.h>

class StorageHealth {
 public:
  bool begin(bool allowUnconfiguredSetupRepair = false);
  bool available() const { return available_; }

 private:
  SPIClass sdSpi_ = SPIClass(VSPI);
  bool available_ = false;
};
