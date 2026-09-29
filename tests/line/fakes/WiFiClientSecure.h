#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
struct FakeSocketState { std::string response; bool connectOk; std::string request; };
FakeSocketState& fakeSocket();
class WiFiClientSecure {
 public:
  void setCACert(const char* cert);
  void setHandshakeTimeout(unsigned long seconds);
  int setTimeout(uint32_t seconds);
  int connect(const char* host,uint16_t port,int32_t timeout);
  bool connected();
  size_t write(const uint8_t* data,size_t len);
  int available();
  int read();
  int read(uint8_t* dst,size_t len);
  void stop();
 private: size_t at_=0; bool connected_=false;
};
