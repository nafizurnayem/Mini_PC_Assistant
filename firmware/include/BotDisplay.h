#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

enum class BotMood {
  Boot,
  Provisioning,
  Connecting,
  Happy,
  Disconnected,
  Waking,
  ShuttingDown,
  SpeedTest,
  Warning,
  Critical,
  Resetting,
  Petted,
  Sleeping,
};

class BotDisplay {
 public:
  BotDisplay();
  void begin();
  void setMood(BotMood mood, const String& message = "");
  void setMetrics(float cpuTemp, float gpuTemp, float memoryPercent);
  void setProvisioningInfo(const String& ssid, const String& password,
                           const String& url);
  void tick(uint32_t now);
  void noteActivity(uint32_t now);
  BotMood mood() const { return mood_; }

 private:
  void drawChrome();
  void drawHeader();
  void drawMain(uint32_t now);
  void drawFace(uint32_t now, uint16_t accent);
  void drawMetrics();
  void drawMetricCard(int16_t x, const char* label, float value,
                      const char* unit, uint16_t accent);
  void drawFooter(uint32_t now);
  uint16_t moodAccent() const;
  const char* moodLabel() const;
  const char* moodHint() const;

  TFT_eSPI display_;
  TFT_eSprite mainSprite_;
  BotMood mood_ = BotMood::Boot;
  String message_;
  float cpuTemp_ = NAN;
  float gpuTemp_ = NAN;
  float memoryPercent_ = NAN;
  String setupSsid_;
  String setupPassword_;
  String setupUrl_;
  uint32_t lastFrame_ = 0;
  uint32_t lastActivity_ = 0;
  uint32_t transitionStarted_ = 0;
  uint32_t frame_ = 0;
  bool spriteReady_ = false;
  bool chromeDirty_ = true;
  bool metricsDirty_ = true;
};
