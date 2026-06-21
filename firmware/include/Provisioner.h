#pragma once

#include "DeviceConfig.h"

class Provisioner {
 public:
  void begin(DeviceConfig* config, ConfigStore* store);
  void stop();
  bool applyConfigJson(const String& json, String& status);
  bool active() const { return active_; }
  bool rebootRequested() const { return rebootRequested_; }

 private:
  DeviceConfig* config_ = nullptr;
  ConfigStore* store_ = nullptr;
  bool active_ = false;
  volatile bool rebootRequested_ = false;
};
