#pragma once

#include <DNSServer.h>
#include <WebServer.h>

#include "DeviceConfig.h"

class Provisioner {
 public:
  void begin(DeviceConfig* config, ConfigStore* store);
  void tick();
  void stop();
  bool applyConfigJson(const String& json, String& status);
  bool active() const { return active_; }
  bool rebootRequested() const { return rebootRequested_; }
  const String& apSsid() const { return apSsid_; }
  const String& apPassword() const { return apPassword_; }
  String setupUrl() const;

 private:
  void startAccessPoint();
  void startWebPortal();
  void handleConfig();
  void handleScan();
  void handleSave();
  void redirectToPortal();

  DeviceConfig* config_ = nullptr;
  ConfigStore* store_ = nullptr;
  DNSServer dnsServer_;
  WebServer webServer_{80};
  String apSsid_;
  String apPassword_;
  bool active_ = false;
  volatile bool rebootRequested_ = false;
};
