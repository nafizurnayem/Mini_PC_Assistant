#include <Arduino.h>
#include <ArduinoJson.h>
#include <MQTT.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <time.h>

#include "BoardConfig.h"
#include "BotDisplay.h"
#include "DeviceConfig.h"
#include "Provisioner.h"

namespace {
DeviceConfig config;
ConfigStore configStore;
BotDisplay bot;
Provisioner provisioner;
WiFiClientSecure tlsClient;
MQTTClient mqtt(2048);
WiFiUDP udp;

String rootTopic;
bool pcOnline = false;
bool touchWasDown = false;
bool touchHeld = false;
uint32_t touchStarted = 0;
uint32_t lastMqttAttempt = 0;
uint32_t lastTelemetry = 0;
String recentCommands[10];
uint8_t recentIndex = 0;

struct PendingCommand {
  bool active = false;
  String commandId;
  String topic;
  String action;
};
PendingCommand pending;

String topic(const char* suffix) { return rootTopic + "/" + suffix; }

String isoNow() {
  time_t now = time(nullptr);
  struct tm utc;
  gmtime_r(&now, &utc);
  char value[25];
  strftime(value, sizeof(value), "%Y-%m-%dT%H:%M:%SZ", &utc);
  return value;
}

bool parseIsoUtc(const String& value, time_t& result) {
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
  int consumed = 0;
  if (sscanf(value.c_str(), "%d-%d-%dT%d:%d:%d%n", &year, &month, &day,
             &hour, &minute, &second, &consumed) != 6 ||
      year < 2024 || month < 1 || month > 12 ||
      day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60) {
    return false;
  }
  while (value[consumed] >= '0' && value[consumed] <= '9') ++consumed;
  if (value[consumed] == '.') {
    ++consumed;
    const int fractionStart = consumed;
    while (value[consumed] >= '0' && value[consumed] <= '9') ++consumed;
    if (consumed == fractionStart) return false;
  }
  if (value[consumed] != 'Z' || value[consumed + 1] != '\0') return false;
  struct tm utc = {};
  utc.tm_year = year - 1900;
  utc.tm_mon = month - 1;
  utc.tm_mday = day;
  utc.tm_hour = hour;
  utc.tm_min = minute;
  utc.tm_sec = second;
  utc.tm_isdst = 0;
  result = mktime(&utc);
  return result > 0;
}

bool duplicateCommand(const String& commandId) {
  for (const auto& seen : recentCommands) {
    if (seen == commandId) return true;
  }
  recentCommands[recentIndex++ % 10] = commandId;
  return false;
}

bool parseMac(const String& value, uint8_t mac[6]) {
  unsigned int bytes[6];
  if (sscanf(value.c_str(), "%x:%x:%x:%x:%x:%x", &bytes[0], &bytes[1],
             &bytes[2], &bytes[3], &bytes[4], &bytes[5]) != 6) return false;
  for (int i = 0; i < 6; ++i) mac[i] = static_cast<uint8_t>(bytes[i]);
  return true;
}

bool sendWakeOnLan() {
  uint8_t mac[6];
  if (!parseMac(config.pcMac, mac)) return false;
  uint8_t packet[102];
  memset(packet, 0xFF, 6);
  for (int i = 1; i <= 16; ++i) memcpy(packet + i * 6, mac, 6);
  IPAddress broadcast;
  if (!broadcast.fromString(config.broadcastIp)) return false;
  udp.begin(9);
  for (int i = 0; i < 3; ++i) {
    udp.beginPacket(broadcast, 9);
    udp.write(packet, sizeof(packet));
    udp.endPacket();
    delay(100);
  }
  udp.stop();
  return true;
}

void publishJson(const String& target, JsonDocument& doc, bool retained = false) {
  String payload;
  serializeJson(doc, payload);
  mqtt.publish(target, payload, retained, 1);
}

void acknowledge(const String& commandId, const char* status, const char* message) {
  JsonDocument doc;
  doc["commandId"] = commandId;
  doc["source"] = "esp32";
  doc["status"] = status;
  doc["message"] = message;
  doc["timestamp"] = isoNow();
  publishJson(topic("event/ack"), doc);
}

void publishPowerCommand(const char* action, const char* source) {
  JsonDocument doc;
  String id = String(static_cast<uint32_t>(esp_random()), HEX) + "-" + String(millis());
  time_t now = time(nullptr);
  doc["commandId"] = id;
  doc["source"] = source;
  doc["action"] = action;
  doc["issuedAt"] = isoNow();
  now += 30;
  struct tm utc;
  gmtime_r(&now, &utc);
  char expiry[25];
  strftime(expiry, sizeof(expiry), "%Y-%m-%dT%H:%M:%SZ", &utc);
  doc["expiresAt"] = expiry;
  publishJson(topic("command/power"), doc);
}

void beep(uint16_t frequency, uint16_t duration) {
  tone(BoardConfig::kBuzzerPin, frequency, duration);
}

void onMessage(String& incomingTopic, String& payload) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) return;

  if (incomingTopic == topic("state/pc")) {
    pcOnline = String(doc["status"] | "offline") == "online";
    if (pcOnline) bot.setMood(BotMood::Happy);
    return;
  }

  if (incomingTopic == topic("telemetry/system")) {
    lastTelemetry = millis();
    bot.setMetrics(doc["cpu"]["temperatureC"] | NAN,
                   doc["gpu"]["temperatureC"] | NAN,
                   doc["memory"]["usedPercent"] | NAN);
    return;
  }

  if (incomingTopic == topic("event/alert")) {
    String severity = doc["severity"] | "warning";
    bot.setMood(severity == "critical" ? BotMood::Critical : BotMood::Warning,
                doc["message"] | "System alert");
    beep(severity == "critical" ? 2100 : 1200, severity == "critical" ? 900 : 300);
    return;
  }

  String commandId = doc["commandId"] | "";
  if (commandId.isEmpty() || duplicateCommand(commandId)) return;

  time_t expiresAt;
  if (!parseIsoUtc(String(doc["expiresAt"] | ""), expiresAt)) {
    acknowledge(commandId, "rejected", "Invalid command expiry");
    return;
  }
  if (expiresAt <= time(nullptr)) {
    acknowledge(commandId, "rejected", "Command expired");
    return;
  }

  if (!pending.active) {
    pending.active = true;
    pending.commandId = commandId;
    pending.topic = incomingTopic;
    pending.action = String(doc["action"] | "");
  }
}

void handlePendingCommand() {
  if (!pending.active) return;
  PendingCommand command = pending;
  pending.active = false;
  if (command.topic == topic("command/power") && command.action == "on") {
    bot.setMood(BotMood::Waking);
    if (sendWakeOnLan()) {
      acknowledge(command.commandId, "accepted", "Wake-on-LAN packet sent");
      beep(1500, 120);
    } else {
      acknowledge(command.commandId, "failed", "Invalid MAC or broadcast address");
      bot.setMood(BotMood::Warning, "WOL failed");
    }
  } else if (command.topic == topic("command/power") && command.action == "off") {
    bot.setMood(BotMood::ShuttingDown);
  } else if (command.topic == topic("command/speedtest")) {
    bot.setMood(BotMood::SpeedTest);
  } else if (command.topic == topic("command/reset")) {
    if (command.action != "wifi" && command.action != "factory") {
      acknowledge(command.commandId, "rejected", "Unknown reset action");
      return;
    }
    acknowledge(command.commandId, "accepted",
                command.action == "wifi" ? "Wi-Fi reset" : "Factory reset");
    bot.setMood(BotMood::Resetting);
    delay(300);
    if (command.action == "wifi") configStore.clearWifi();
    else configStore.clearAll();
    ESP.restart();
  }
}

void connectMqtt(uint32_t now) {
  if (mqtt.connected() || now - lastMqttAttempt < BoardConfig::kReconnectMs) return;
  if (time(nullptr) < 1700000000) {
    bot.setMood(BotMood::Connecting, "Syncing clock");
    return;
  }
  lastMqttAttempt = now;
  String clientId = "pcbot-esp32-" + config.deviceId;
  String willTopic = topic("state/bot");
  String willPayload = "{\"status\":\"offline\",\"source\":\"esp32\"}";
  mqtt.setWill(willTopic.c_str(), willPayload.c_str(), true, 1);
  if (!mqtt.connect(clientId.c_str(), config.mqttUsername.c_str(),
                    config.mqttPassword.c_str())) {
    bot.setMood(BotMood::Disconnected, "MQTT offline");
    return;
  }
  mqtt.subscribe(topic("command/power"), 1);
  mqtt.subscribe(topic("command/speedtest"), 1);
  mqtt.subscribe(topic("command/reset"), 1);
  mqtt.subscribe(topic("state/pc"), 1);
  mqtt.subscribe(topic("telemetry/system"), 1);
  mqtt.subscribe(topic("event/alert"), 1);
  mqtt.publish(topic("state/bot"), "{\"status\":\"online\",\"source\":\"esp32\"}", true, 1);
  bot.setMood(BotMood::Happy);
}

void handleTouch(uint32_t now) {
  bool down = digitalRead(BoardConfig::kTouchPin) == HIGH;
  if (down && !touchWasDown) {
    touchStarted = now;
    touchHeld = false;
    bot.noteActivity(now);
  }
  if (down && !touchHeld && now - touchStarted >= BoardConfig::kTouchHoldMs) {
    touchHeld = true;
    publishPowerCommand(pcOnline ? "off" : "on", "bot");
    bot.setMood(pcOnline ? BotMood::ShuttingDown : BotMood::Waking);
    beep(1800, 250);
  }
  if (!down && touchWasDown && !touchHeld) {
    bot.setMood(BotMood::Petted);
    beep(2200, 70);
  }
  touchWasDown = down;
}
}

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println("\n[pcbot] boot");
  Serial.printf("[pcbot] flash=%u bytes, psram=%u bytes\n",
                ESP.getFlashChipSize(), ESP.getPsramSize());
  setenv("TZ", "UTC0", 1);
  tzset();
  pinMode(BoardConfig::kTouchPin, INPUT);
  pinMode(BoardConfig::kBuzzerPin, OUTPUT);
  bot.begin();
  bot.setMood(BotMood::Boot);

  if (digitalRead(BoardConfig::kTouchPin) == HIGH) {
    const uint32_t recoveryStarted = millis();
    bot.setMood(BotMood::Resetting, "Hold 5s for Wi-Fi setup");
    while (digitalRead(BoardConfig::kTouchPin) == HIGH &&
           millis() - recoveryStarted < 5000) {
      bot.tick(millis());
      delay(5);
    }
    if (digitalRead(BoardConfig::kTouchPin) == HIGH) {
      configStore.clearWifi();
      beep(1800, 250);
      bot.setMood(BotMood::Provisioning, "Wi-Fi setup enabled");
      delay(350);
    }
  }

  configStore.load(config);
  rootTopic = "pcbot/v1/" + config.deviceId;

  if (!config.ready()) {
    Serial.println("[pcbot] configuration required");
    bot.setMood(BotMood::Provisioning);
    provisioner.begin(&config, &configStore);
    bot.setProvisioningInfo(provisioner.apSsid(), provisioner.apPassword(),
                            provisioner.setupUrl());
    return;
  }

  bot.setMood(BotMood::Connecting, "Wi-Fi");
  Serial.printf("[wifi] connecting to %s\n", config.wifiSsid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());
  tlsClient.setCACert(config.rootCa.c_str());
  mqtt.begin(config.mqttHost.c_str(), config.mqttPort, tlsClient);
  mqtt.onMessage(onMessage);
  configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
}

void loop() {
  uint32_t now = millis();
  bot.tick(now);
  if (provisioner.active()) {
    provisioner.tick();
    if (provisioner.rebootRequested()) {
      delay(500);
      ESP.restart();
    }
    delay(5);
    return;
  }

  handleTouch(now);
  if (WiFi.status() != WL_CONNECTED) {
    bot.setMood(BotMood::Disconnected, "Wi-Fi offline");
    delay(10);
    return;
  }
  connectMqtt(now);
  mqtt.loop();
  handlePendingCommand();

  if (lastTelemetry && now - lastTelemetry > BoardConfig::kTelemetryLostMs && pcOnline) {
    bot.setMood(BotMood::Warning, "PC data lost");
  }
  delay(5);
}
