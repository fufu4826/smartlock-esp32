#pragma once

#include <cstdint>

class MDNSMock {
 public:
  bool begin(const char* name);
  void end();
  bool addService(const char* service, const char* proto, uint16_t port);
  bool addServiceTxt(char* service, char* proto, char* key, char* value);
};

extern MDNSMock MDNS;
