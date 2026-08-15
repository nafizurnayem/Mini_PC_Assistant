#include "DeviceConfig.h"

#include <Preferences.h>

namespace {
Preferences prefs;
}

bool ConfigStore::load(DeviceConfig& config) {
  if (!prefs.begin("pcbot", true)) return false;
  config.deviceId = prefs.getString("device", "desktop-01");
  config.wifiSsid = prefs.getString("ssid", "");
  config.wifiPassword = prefs.getString("wifiPass", "");
  config.mqttHost = prefs.getString("mqttHost", "");
  config.mqttPort = prefs.getUShort("mqttPort", 8883);
  config.mqttUsername = prefs.getString("mqttUser", "");
  config.mqttPassword = prefs.getString("mqttPass", "");
  config.pcMac = prefs.getString("pcMac", "");
  config.broadcastIp = prefs.getString("broadcast", "255.255.255.255");
  config.rootCa = prefs.getString("rootCa", "");
  config.timezone = prefs.getString("tz", "UTC0");
  config.otaToken = prefs.getString("otaToken", "");
  prefs.end();
  return config.ready();
}

bool ConfigStore::save(const DeviceConfig& config) {
  if (!prefs.begin("pcbot", false)) return false;
  prefs.putString("device", config.deviceId);
  prefs.putString("ssid", config.wifiSsid);
  prefs.putString("wifiPass", config.wifiPassword);
  prefs.putString("mqttHost", config.mqttHost);
  prefs.putUShort("mqttPort", config.mqttPort);
  prefs.putString("mqttUser", config.mqttUsername);
  prefs.putString("mqttPass", config.mqttPassword);
  prefs.putString("pcMac", config.pcMac);
  prefs.putString("broadcast", config.broadcastIp);
  prefs.putString("rootCa", config.rootCa);
  prefs.putString("tz", config.timezone);
  prefs.putString("otaToken", config.otaToken);
  prefs.end();
  return true;
}

void ConfigStore::clearWifi() {
  if (!prefs.begin("pcbot", false)) return;
  prefs.remove("ssid");
  prefs.remove("wifiPass");
  prefs.end();
}

void ConfigStore::clearAll() {
  if (!prefs.begin("pcbot", false)) return;
  prefs.clear();
  prefs.end();
}

