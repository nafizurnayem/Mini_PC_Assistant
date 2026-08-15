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
#include "OtaServer.h"
#include "Provisioner.h"

namespace {
DeviceConfig config;
ConfigStore configStore;
BotDisplay bot;
Provisioner provisioner;
WiFiClientSecure tlsClient;
MQTTClient mqtt(2048);
WiFiUDP udp;
OtaServer ota;
bool otaStarted = false;

String rootTopic;
bool pcOnline = false;

// Debounced TTP223 state. raw/stable are separated so contact bounce and
// electrical noise can never be mistaken for a tap.
struct TouchState {
  bool raw = false;
  bool stable = false;
  bool holdFired = false;
  uint32_t lastEdge = 0;
  uint32_t pressedAt = 0;
};
TouchState touch;

uint32_t lastMqttAttempt = 0;
uint32_t lastWifiAttempt = 0;
uint32_t lastTelemetry = 0;
uint32_t lastRssiSample = 0;

// Volume knob state. The smoothed reading is kept in ADC counts so the
// percentage only ever changes when the knob really moved.
float potFiltered = NAN;
int16_t lastSentVolume = -1;
uint32_t lastVolumeSample = 0;
uint32_t lastVolumePublish = 0;
String recentCommands[10];
uint8_t recentIndex = 0;

struct PendingCommand {
  bool active = false;
  String commandId;
  String topic;
  String action;
};
PendingCommand pending;

// Potentiometer state — tracks the smoothed ADC reading and the last
// volume level that was sent to the broker so we can apply a deadband.
struct PotState {
  float smoothed = -1.0f;   // EMA of raw ADC; -1 = not yet seeded
  int lastPublished = -1;   // volume % last sent; -1 = never
  uint32_t lastSampleMs = 0;
  uint32_t lastPublishMs = 0;
};
PotState pot;

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
  recentCommands[recentIndex] = commandId;
  recentIndex = (recentIndex + 1) % 10;
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

// While the PC is off there is no telemetry to render and no commands to run,
// so the controller drops to the lowest frequency Wi-Fi still supports.
void applyPowerProfile(bool pcIsOn) {
  const uint32_t target =
      pcIsOn ? BoardConfig::kActiveCpuMhz : BoardConfig::kPowerSaverCpuMhz;
  if (getCpuFrequencyMhz() == target) return;
  setCpuFrequencyMhz(target);
  Serial.printf("[pico] cpu %u MHz (pc %s)\n", target, pcIsOn ? "on" : "off");
}

void onMessage(String& incomingTopic, String& payload) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) return;

  if (incomingTopic == topic("state/pc")) {
    pcOnline = String(doc["status"] | "offline") == "online";
    if (pcOnline) bot.setMood(BotMood::Happy);
    bot.setPcOnline(pcOnline);
    return;
  }

  if (incomingTopic == topic("telemetry/system")) {
    lastTelemetry = millis();
    bot.setMetrics(doc["cpu"]["temperatureC"] | NAN,
                   doc["gpu"]["temperatureC"] | NAN,
                   doc["memory"]["usedPercent"] | NAN);
    bot.setNetwork(doc["network"]["downloadMbps"] | NAN,
                   doc["network"]["uploadMbps"] | NAN,
                   WiFi.status() == WL_CONNECTED
                       ? static_cast<int16_t>(WiFi.RSSI())
                       : 0);
    return;
  }

  if (incomingTopic == topic("state/media")) {
    bot.setMedia(doc["status"] | "stopped", doc["title"] | "",
                 doc["artist"] | "", doc["app"] | "",
                 doc["positionSeconds"] | NAN, doc["durationSeconds"] | NAN);
    return;
  }

  if (incomingTopic == topic("state/volume")) {
    const int level = doc["level"] | 0;
    bot.setVolume(static_cast<uint8_t>(constrain(level, 0, 100)),
                  doc["muted"] | false);
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

// Everything the desktop app needs to find this device and push firmware to it.
// The OTA token rides along because every client on this broker is already
// trusted with commands that can shut the PC down.
void publishBotState() {
  JsonDocument doc;
  doc["status"] = "online";
  doc["source"] = "esp32";
  doc["version"] = BoardConfig::kFirmwareVersion;
  doc["ip"] = WiFi.localIP().toString();
  doc["otaToken"] = config.otaToken;
  doc["timestamp"] = isoNow();
  publishJson(topic("state/bot"), doc, true);
}

void connectMqtt(uint32_t now) {
  if (mqtt.connected() || now - lastMqttAttempt < BoardConfig::kReconnectMs) return;
  if (time(nullptr) < 1700000000) {
    bot.setMood(BotMood::Connecting, "Syncing clock");
    return;
  }
  lastMqttAttempt = now;
  // The TLS handshake blocks for up to a few seconds, so tell the operator what
  // is happening before the loop stalls. The client id keeps its original
  // prefix because broker ACLs are usually written against it.
  bot.setMood(BotMood::Connecting, "Reaching the broker");
  bot.tick(now);
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
  mqtt.subscribe(topic("state/media"), 1);
  mqtt.subscribe(topic("state/volume"), 1);
  mqtt.subscribe(topic("telemetry/system"), 1);
  mqtt.subscribe(topic("event/alert"), 1);
  publishBotState();
  bot.setMood(BotMood::Happy);
}

// Reads the knob, smooths it, and publishes a setpoint only when the operator
// actually moved it. Volume is an absolute, continuously-updated value, so it
// goes out at QoS 0 without retention: a stale setpoint is worthless.
void handleVolumeKnob(uint32_t now) {
  if (now - lastVolumeSample < BoardConfig::kVolumeSampleMs) return;
  lastVolumeSample = now;

  const float raw = static_cast<float>(analogRead(BoardConfig::kPotPin));
  potFiltered = isnan(potFiltered) ? raw : potFiltered + (raw - potFiltered) * 0.25f;
  if (now < BoardConfig::kVolumeSettleMs) return;

  int level = static_cast<int>(potFiltered * 100.0f / BoardConfig::kPotAdcMax + 0.5f);
  level = constrain(level, 0, 100);

  const bool endStop = (level == 0 || level == 100) && level != lastSentVolume;
  const bool moved = lastSentVolume < 0 ||
                     abs(level - lastSentVolume) >= BoardConfig::kVolumeStepPercent;
  if (!moved && !endStop) return;
  if (now - lastVolumePublish < BoardConfig::kVolumePublishMs) return;
  if (!mqtt.connected()) return;

  lastSentVolume = level;
  lastVolumePublish = now;

  JsonDocument doc;
  doc["level"] = level;
  doc["source"] = "bot";
  doc["timestamp"] = isoNow();
  String payload;
  serializeJson(doc, payload);
  mqtt.publish(topic("command/volume"), payload, false, 0);
  // Show it immediately rather than waiting for the PC to echo the change back.
  bot.setVolume(static_cast<uint8_t>(level), level == 0);
}

bool touchIsActive() {
  const int level = digitalRead(BoardConfig::kTouchPin);
  return BoardConfig::kTouchActiveHigh ? level == HIGH : level == LOW;
}

void onTouchHold() {
  // Power control needs a broker session, so during setup the hold only
  // explains itself instead of silently doing nothing.
  if (provisioner.active()) {
    beep(900, 120);
    return;
  }
  if (!mqtt.connected()) {
    bot.setMood(BotMood::Disconnected, "No link for power control");
    beep(900, 120);
    return;
  }
  publishPowerCommand(pcOnline ? "off" : "on", "bot");
  bot.setMood(pcOnline ? BotMood::ShuttingDown : BotMood::Waking);
  beep(1800, 250);
  Serial.printf("[pico] hold: power %s\n", pcOnline ? "off" : "on");
}

void onTouchTap() {
  bot.setMood(BotMood::Petted);
  beep(2200, 70);
  Serial.println("[pico] tap");
}

// Reads the potentiometer and publishes a VolumeSetpoint whenever the knob
// moves by >= 2 steps. Uses EMA smoothing to suppress ADC noise, and a
// minimum 80 ms inter-publish gap to avoid flooding the broker.
void handlePotentiometer(uint32_t now) {
  if (now - pot.lastSampleMs < 50) return;  // 20 Hz sampling
  pot.lastSampleMs = now;

  const int raw = analogRead(BoardConfig::kPotPin);
  // Seed the EMA on first read so the display doesn't animate from zero.
  if (pot.smoothed < 0.0f) pot.smoothed = static_cast<float>(raw);
  pot.smoothed = pot.smoothed * 0.85f + static_cast<float>(raw) * 0.15f;

  // Map 12-bit ADC (0-4095) to volume 0-100.
  const int level = static_cast<int>(pot.smoothed * 100.0f / 4095.0f + 0.5f);

  // Deadband: ignore tiny jitter; only publish when knob moves noticeably.
  if (abs(level - pot.lastPublished) < 2) return;
  if (!mqtt.connected()) return;
  if (now - pot.lastPublishMs < 80) return;   // rate limit: max ~12 msg/s

  pot.lastPublished = level;
  pot.lastPublishMs = now;

  JsonDocument doc;
  doc["level"]     = level;
  doc["source"]    = "esp32";
  doc["timestamp"] = isoNow();
  publishJson(topic("command/volume"), doc, false);
  Serial.printf("[pico] volume knob -> %d%%\n", level);
}

void handleTouch(uint32_t now) {
  const bool raw = touchIsActive();
  if (raw != touch.raw) {
    touch.raw = raw;
    touch.lastEdge = now;
    // Logged before debouncing: this separates "the sensor never fires" from
    // "the sensor fires but the gesture was too short".
    Serial.printf("[pico] touch raw=%d\n", raw ? 1 : 0);
  }

  const bool settled = now - touch.lastEdge >= BoardConfig::kTouchDebounceMs;
  if (settled && raw != touch.stable) {
    touch.stable = raw;
    if (raw) {
      touch.pressedAt = now;
      touch.holdFired = false;
      bot.noteActivity(now);
      beep(2600, 18);
    } else {
      const uint32_t heldFor = now - touch.pressedAt;
      if (!touch.holdFired && heldFor <= BoardConfig::kTouchTapMaxMs) onTouchTap();
    }
  }

  float progress = 0.0f;
  if (touch.stable) {
    const uint32_t heldFor = now - touch.pressedAt;
    progress = static_cast<float>(heldFor) / BoardConfig::kTouchHoldMs;
    if (progress > 1.0f) progress = 1.0f;
    if (!touch.holdFired && heldFor >= BoardConfig::kTouchHoldMs) {
      touch.holdFired = true;
      onTouchHold();
    }
  }
  bot.setTouch(touch.stable, progress);
}
}

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.printf("\n[pico] %s boot\n", BoardConfig::kProductFullName);
  Serial.printf("[pico] flash=%u bytes, psram=%u bytes\n",
                ESP.getFlashChipSize(), ESP.getPsramSize());
  setenv("TZ", "UTC0", 1);
  tzset();
  pinMode(BoardConfig::kTouchPin, BoardConfig::kTouchPinMode);
  pinMode(BoardConfig::kBuzzerPin, OUTPUT);
  // Full-scale attenuation so the knob covers the whole 0-3.3 V swing.
  analogSetPinAttenuation(BoardConfig::kPotPin, ADC_11db);
  // ADC1_CH2 (GPIO 3) for the volume potentiometer.
  // 11 dB attenuation → full 0–3.3 V input range on ESP32-S3.
  analogSetAttenuation(ADC_11db);
  pinMode(BoardConfig::kPotPin, INPUT);
  Serial.printf("[pico] touch pin %u idle level=%d (active %s)\n",
                BoardConfig::kTouchPin, digitalRead(BoardConfig::kTouchPin),
                BoardConfig::kTouchActiveHigh ? "high" : "low");
  bot.begin();
  bot.setMood(BotMood::Boot);

  if (touchIsActive()) {
    const uint32_t recoveryStarted = millis();
    bot.setMood(BotMood::Resetting, "Hold 5s for Wi-Fi setup");
    while (touchIsActive() && millis() - recoveryStarted < 5000) {
      bot.tick(millis());
      delay(5);
    }
    if (touchIsActive()) {
      configStore.clearWifi();
      beep(1800, 250);
      bot.setMood(BotMood::Provisioning, "Wi-Fi setup enabled");
      delay(350);
    }
  }

  configStore.load(config);
  rootTopic = "pcbot/v1/" + config.deviceId;
  // Mint the OTA secret once, on first boot after an update or a factory reset.
  if (config.otaToken.length() < BoardConfig::kOtaTokenBytes) {
    String token;
    for (uint32_t i = 0; i < BoardConfig::kOtaTokenBytes; ++i) {
      char pair[3];
      snprintf(pair, sizeof(pair), "%02x",
               static_cast<unsigned>(esp_random() & 0xFF));
      token += pair;
    }
    config.otaToken = token;
    configStore.save(config);
    Serial.println("[ota] generated a new update token");
  }
  // MQTT timestamps stay UTC (they use gmtime); this only drives the clock.
  setenv("TZ", config.timezone.c_str(), 1);
  tzset();
  Serial.printf("[pico] timezone %s\n", config.timezone.c_str());

  if (!config.ready()) {
    Serial.println("[pico] configuration required");
    bot.setMood(BotMood::Provisioning);
    provisioner.begin(&config, &configStore);
    bot.setProvisioningInfo(provisioner.apSsid(), provisioner.apPassword(),
                            provisioner.setupUrl());
    return;
  }

  bot.setMood(BotMood::Connecting, "Wi-Fi");
  Serial.printf("[wifi] connecting to %s\n", config.wifiSsid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());
  lastWifiAttempt = millis();
  tlsClient.setCACert(config.rootCa.c_str());
  mqtt.begin(config.mqttHost.c_str(), config.mqttPort, tlsClient);
  mqtt.onMessage(onMessage);
  configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
}

void loop() {
  uint32_t now = millis();
  // Touch is sampled first and on every path so the pad stays alive during
  // setup mode, Wi-Fi outages and broker retries.
  handleTouch(now);
  handlePotentiometer(now);
  bot.tick(now);
  // The display owns the power-saver decision; the clock screen is the single
  // signal for when the controller may slow down.
  applyPowerProfile(!bot.powerSaverActive());

  if (provisioner.active()) {
    provisioner.tick();
    if (provisioner.rebootRequested()) {
      delay(500);
      ESP.restart();
    }
    delay(2);
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    bot.setMood(BotMood::Disconnected, "Wi-Fi offline");
    if (now - lastWifiAttempt >= BoardConfig::kWifiRetryMs) {
      lastWifiAttempt = now;
      Serial.println("[wifi] retrying association");
      WiFi.disconnect();
      WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());
    }
    delay(2);
    return;
  }
  // The update server only exists once the device is on the LAN.
  if (!otaStarted) {
    otaStarted = true;
    ota.begin(config.otaToken, BoardConfig::kFirmwareVersion, config.deviceId,
              [](uint8_t percent) { bot.setUpdateProgress(percent); });
  }
  ota.tick();
  if (ota.active()) {
    // Nothing else matters while firmware is being written.
    delay(1);
    return;
  }

  connectMqtt(now);
  mqtt.loop();
  handlePendingCommand();
  handleVolumeKnob(now);

  // Refresh link strength between telemetry messages so the network tile stays
  // live even while the PC is quiet.
  if (now - lastRssiSample >= 2000) {
    lastRssiSample = now;
    bot.setNetwork(NAN, NAN, static_cast<int16_t>(WiFi.RSSI()));
  }

  if (lastTelemetry && now - lastTelemetry > BoardConfig::kTelemetryLostMs && pcOnline) {
    bot.setMood(BotMood::Warning, "PC data lost");
  }
  delay(2);
}
