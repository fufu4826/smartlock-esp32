#pragma once
#include <Arduino.h>
class AdminPin {
 public:
  enum class Result { Ok, Invalid, Locked, StorageFailure };
  using FailureCallback = void (*)(uint8_t count, bool lockout);
  static void setFailureCallback(FailureCallback callback);
  static bool begin(bool configured, bool freshInstallation = false);
  static bool valid(const char* pin);
  static bool initialize(const char* pin);
  static Result verify(const char* pin,uint32_t now);
  static Result change(const char* oldPin,const char* next,const char* confirm,uint32_t now);
  static uint32_t revision(){return revision_;}
 private:
  static bool save(const char* pin);
  static bool present_, healthy_, initialDefault_;
  static uint8_t failures_;
  static uint32_t lockedAt_, revision_;
};
