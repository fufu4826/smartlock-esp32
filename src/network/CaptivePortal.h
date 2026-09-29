#pragma once

#include <DNSServer.h>
#include <IPAddress.h>

class CaptivePortal {
 public:
  bool begin(IPAddress portalIp);
  void update();

 private:
  DNSServer dns_;
  bool started_ = false;
};
