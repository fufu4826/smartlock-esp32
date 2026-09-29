#include "CaptivePortal.h"

bool CaptivePortal::begin(IPAddress portalIp) {
  dns_.setErrorReplyCode(DNSReplyCode::NoError);
  started_ = dns_.start(53, "*", portalIp);
  return started_;
}

void CaptivePortal::update() {
  if (started_) dns_.processNextRequest();
}
