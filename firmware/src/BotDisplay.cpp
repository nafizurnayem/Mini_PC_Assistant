#include "BotDisplay.h"

#include "BoardConfig.h"
#include <Wire.h>

BotDisplay::BotDisplay()
    : display_(U8G2_R0, U8X8_PIN_NONE) {}

void BotDisplay::begin() {
  Wire.begin(BoardConfig::kOledSda, BoardConfig::kOledScl);
  display_.setI2CAddress(BoardConfig::kOledAddress << 1);
  display_.begin();
  display_.setContrast(180);
  lastActivity_ = millis();
}

void BotDisplay::setMood(BotMood mood, const String& message) {
  mood_ = mood;
  message_ = message;
  noteActivity(millis());
}

void BotDisplay::setMetrics(float cpuTemp, float gpuTemp, float memoryPercent) {
  cpuTemp_ = cpuTemp;
  gpuTemp_ = gpuTemp;
  memoryPercent_ = memoryPercent;
}

void BotDisplay::noteActivity(uint32_t now) {
  lastActivity_ = now;
  display_.setContrast(180);
}

void BotDisplay::drawEyes(int8_t xOffset, int8_t yOffset, bool closed) {
  const int left = 38 + xOffset;
  const int right = 82 + xOffset;
  if (closed) {
    display_.drawLine(left - 7, 25 + yOffset, left + 7, 25 + yOffset);
    display_.drawLine(right - 7, 25 + yOffset, right + 7, 25 + yOffset);
  } else {
    display_.drawRFrame(left - 9, 17 + yOffset, 18, 18, 5);
    display_.drawRFrame(right - 9, 17 + yOffset, 18, 18, 5);
    display_.drawDisc(left + ((frame_ / 3) % 3) - 1, 26 + yOffset, 3);
    display_.drawDisc(right + ((frame_ / 3) % 3) - 1, 26 + yOffset, 3);
  }
}

void BotDisplay::drawFace(uint32_t now) {
  const int8_t drift = ((now / 10000) % 3) - 1;
  bool closed = mood_ == BotMood::Sleeping || (frame_ % 45 == 0);
  drawEyes(drift, 0, closed);

  switch (mood_) {
    case BotMood::Happy:
    case BotMood::Petted:
      display_.drawLine(55, 38, 60, 42);
      display_.drawLine(60, 42, 68, 42);
      display_.drawLine(68, 42, 73, 38);
      if (mood_ == BotMood::Petted) {
        display_.drawStr(57, 13, "<3");
      }
      break;
    case BotMood::Warning:
      display_.drawTriangle(58, 39, 70, 39, 64, 31);
      break;
    case BotMood::Critical:
      display_.drawLine(53, 39, 75, 39);
      display_.drawLine(53, 40, 75, 40);
      break;
    case BotMood::Sleeping:
      display_.drawStr(91, 19, "z");
      display_.drawStr(101, 11, "Z");
      break;
    default:
      display_.drawCircle(64, 38, 4 + ((frame_ / 2) % 2));
      break;
  }
}

void BotDisplay::tick(uint32_t now) {
  if (now - lastFrame_ < BoardConfig::kDisplayFrameMs) return;
  lastFrame_ = now;
  frame_++;

  if (now - lastActivity_ > BoardConfig::kIdleSleepMs &&
      mood_ != BotMood::Warning && mood_ != BotMood::Critical &&
      mood_ != BotMood::Provisioning && mood_ != BotMood::Disconnected) {
    mood_ = BotMood::Sleeping;
    display_.setContrast(30);
  }

  display_.clearBuffer();
  display_.setFont(u8g2_font_6x12_tr);
  drawFace(now);

  String label = message_;
  if (label.isEmpty()) {
    switch (mood_) {
      case BotMood::Boot: label = "Booting"; break;
      case BotMood::Provisioning: label = "Pair with phone"; break;
      case BotMood::Connecting: label = "Connecting"; break;
      case BotMood::Happy: label = "Ready"; break;
      case BotMood::Disconnected: label = "Offline"; break;
      case BotMood::Waking: label = "Wake PC"; break;
      case BotMood::ShuttingDown: label = "Good night PC"; break;
      case BotMood::SpeedTest: label = "Speed test"; break;
      case BotMood::Warning: label = "Getting warm"; break;
      case BotMood::Critical: label = "Too hot!"; break;
      case BotMood::Resetting: label = "Resetting"; break;
      case BotMood::Petted: label = "Hehe"; break;
      case BotMood::Sleeping: label = ""; break;
    }
  }
  display_.drawUTF8(max(0, (128 - display_.getUTF8Width(label.c_str())) / 2), 55, label.c_str());

  if (mood_ == BotMood::Happy && !isnan(cpuTemp_)) {
    char metrics[24];
    snprintf(metrics, sizeof(metrics), "C%.0f G%.0f R%.0f%%", cpuTemp_, gpuTemp_, memoryPercent_);
    display_.drawStr(2, 64, metrics);
  }
  display_.sendBuffer();
}
