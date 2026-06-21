#pragma once

#include <Arduino.h>
#include <U8g2lib.h>

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
  void tick(uint32_t now);
  void noteActivity(uint32_t now);
  BotMood mood() const { return mood_; }

 private:
  void drawFace(uint32_t now);
  void drawEyes(int8_t xOffset, int8_t yOffset, bool closed);
  U8G2_SH1106_128X64_NONAME_F_HW_I2C display_;
  BotMood mood_ = BotMood::Boot;
  String message_;
  float cpuTemp_ = NAN;
  float gpuTemp_ = NAN;
  float memoryPercent_ = NAN;
  uint32_t lastFrame_ = 0;
  uint32_t lastActivity_ = 0;
  uint8_t frame_ = 0;
};

