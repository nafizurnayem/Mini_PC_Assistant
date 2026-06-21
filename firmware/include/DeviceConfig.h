#pragma once

#include <Arduino.h>

struct DeviceConfig {
  String deviceId = "desktop-01";
  String wifiSsid;
  String wifiPassword;
  String mqttHost;
  uint16_t mqttPort = 8883;
  String mqttUsername;
  String mqttPassword;
  String pcMac;
  String broadcastIp = "255.255.255.255";
  String rootCa;

  bool ready() const {
    return !wifiSsid.isEmpty() && !mqttHost.isEmpty() &&
           !mqttUsername.isEmpty() && !mqttPassword.isEmpty() &&
           !pcMac.isEmpty() && !rootCa.isEmpty();
  }
};

class ConfigStore {
 public:
  bool load(DeviceConfig& config);
  bool save(const DeviceConfig& config);
  void clearWifi();
  void clearAll();
};

