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
  // POSIX TZ string (for example "CET-1CEST,M3.5.0,M10.5.0/3"). Only affects
  // the local clock screen; MQTT timestamps stay UTC.
  String timezone = "UTC0";
  // Random per-device secret guarding the LAN firmware upload endpoint.
  // Generated on first boot and published on state/bot for the desktop app.
  String otaToken;

  // Returns an empty string when valid, or a human-readable description of
  // the first failing field so the provisioner can surface a precise error.
  String validationError() const {
    if (deviceId.length() < 2 || deviceId.length() > 32)
      return "Device ID must be 2–32 characters";
    for (size_t index = 0; index < deviceId.length(); ++index) {
      const char value = deviceId[index];
      if (!isalnum(static_cast<unsigned char>(value)) &&
          value != '-' && value != '_') {
        return "Device ID: only letters, numbers, - and _ allowed (no spaces or apostrophes)";
      }
    }
    if (wifiSsid.isEmpty()) return "Wi-Fi SSID is required";
    if (mqttHost.isEmpty()) return "MQTT broker host is required";
    if (mqttPort == 0) return "MQTT TLS port is invalid";
    if (mqttUsername.isEmpty()) return "MQTT username is required";
    if (mqttPassword.isEmpty()) return "MQTT password is required";

    unsigned int mac[6];
    char trailing;
    const bool validMac =
        sscanf(pcMac.c_str(), "%x:%x:%x:%x:%x:%x%c", &mac[0], &mac[1],
               &mac[2], &mac[3], &mac[4], &mac[5], &trailing) == 6 &&
        mac[0] <= 0xFF && mac[1] <= 0xFF && mac[2] <= 0xFF &&
        mac[3] <= 0xFF && mac[4] <= 0xFF && mac[5] <= 0xFF;
    if (!validMac) return "PC MAC address format must be AA:BB:CC:DD:EE:FF";

    unsigned int ip[4];
    char ipTrailing;
    const bool validBroadcast =
        sscanf(broadcastIp.c_str(), "%u.%u.%u.%u%c", &ip[0], &ip[1],
               &ip[2], &ip[3], &ipTrailing) == 4 &&
        ip[0] <= 255 && ip[1] <= 255 && ip[2] <= 255 && ip[3] <= 255;
    if (!validBroadcast) return "LAN broadcast IP is not a valid IPv4 address";

    const bool validCa =
        rootCa.indexOf("-----BEGIN CERTIFICATE-----") >= 0 &&
        rootCa.indexOf("-----END CERTIFICATE-----") >= 0;
    if (!validCa) return "Root CA certificate must be a valid PEM (paste the full -----BEGIN CERTIFICATE----- block)";

    return "";
  }

  bool ready() const { return validationError().isEmpty(); }
};

class ConfigStore {
 public:
  bool load(DeviceConfig& config);
  bool save(const DeviceConfig& config);
  void clearWifi();
  void clearAll();
};
