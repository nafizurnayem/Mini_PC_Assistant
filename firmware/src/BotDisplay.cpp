#include "BotDisplay.h"

#include "BoardConfig.h"
#include <math.h>

namespace {
constexpr uint16_t kBackground = 0x0023;
constexpr uint16_t kSurface = 0x08A6;
constexpr uint16_t kPanel = 0x1129;
constexpr uint16_t kPanelEdge = 0x2A0D;
constexpr uint16_t kText = 0xEFFF;
constexpr uint16_t kMuted = 0x7414;
constexpr uint16_t kCyan = 0x273F;
constexpr uint16_t kGreen = 0x4FF3;
constexpr uint16_t kAmber = 0xFDC9;
constexpr uint16_t kRed = 0xFA2C;
constexpr uint16_t kMagenta = 0xF9F5;
constexpr int16_t kMainWidth = 304;
constexpr int16_t kMainHeight = 126;

uint16_t dimColor(uint16_t color) {
  return ((color & 0xF79E) >> 1);
}

bool valueChanged(float before, float after) {
  if (isnan(before) != isnan(after)) return true;
  return !isnan(before) && fabsf(before - after) >= 0.1f;
}
}  // namespace

BotDisplay::BotDisplay() : mainSprite_(&display_) {}

void BotDisplay::begin() {
  display_.init();
  display_.setRotation(1);
  display_.setTextDatum(TL_DATUM);

  // Make physical display bring-up unmistakable before the normal UI starts.
  display_.fillScreen(TFT_RED);
  delay(350);
  display_.fillScreen(TFT_GREEN);
  delay(350);
  display_.fillScreen(TFT_BLUE);
  delay(350);
  display_.fillScreen(kBackground);

  mainSprite_.setColorDepth(16);
  spriteReady_ = mainSprite_.createSprite(kMainWidth, kMainHeight) != nullptr;
  if (!spriteReady_) {
    display_.setTextColor(kRed, kBackground);
    display_.drawCentreString("DISPLAY BUFFER ERROR", 160, 105, 2);
    display_.setTextColor(kMuted, kBackground);
    display_.drawCentreString("Restart the controller", 160, 129, 2);
  }

  lastActivity_ = millis();
  transitionStarted_ = lastActivity_;
  drawChrome();
}

void BotDisplay::setMood(BotMood mood, const String& message) {
  if (mood_ == mood && message_ == message) return;
  mood_ = mood;
  message_ = message;
  const uint32_t now = millis();
  transitionStarted_ = now;
  chromeDirty_ = true;
  noteActivity(now);
}

void BotDisplay::setMetrics(float cpuTemp, float gpuTemp, float memoryPercent) {
  if (valueChanged(cpuTemp_, cpuTemp) || valueChanged(gpuTemp_, gpuTemp) ||
      valueChanged(memoryPercent_, memoryPercent)) {
    metricsDirty_ = true;
  }
  cpuTemp_ = cpuTemp;
  gpuTemp_ = gpuTemp;
  memoryPercent_ = memoryPercent;
}

void BotDisplay::setProvisioningInfo(const String& ssid,
                                     const String& password,
                                     const String& url) {
  setupSsid_ = ssid;
  setupPassword_ = password;
  setupUrl_ = url;
  chromeDirty_ = true;
}

void BotDisplay::noteActivity(uint32_t now) {
  lastActivity_ = now;
}

uint16_t BotDisplay::moodAccent() const {
  switch (mood_) {
    case BotMood::Warning:
      return kAmber;
    case BotMood::Critical:
      return kRed;
    case BotMood::Disconnected:
      return kMuted;
    case BotMood::Waking:
    case BotMood::SpeedTest:
      return kMagenta;
    case BotMood::Happy:
    case BotMood::Petted:
      return kGreen;
    default:
      return kCyan;
  }
}

const char* BotDisplay::moodLabel() const {
  switch (mood_) {
    case BotMood::Boot: return "SYSTEM BOOT";
    case BotMood::Provisioning: return "PAIRING MODE";
    case BotMood::Connecting: return "CONNECTING";
    case BotMood::Happy: return "SYSTEM READY";
    case BotMood::Disconnected: return "LINK OFFLINE";
    case BotMood::Waking: return "WAKE SIGNAL";
    case BotMood::ShuttingDown: return "SHUTDOWN";
    case BotMood::SpeedTest: return "SPEED TEST";
    case BotMood::Warning: return "THERMAL WARN";
    case BotMood::Critical: return "CRITICAL";
    case BotMood::Resetting: return "RESETTING";
    case BotMood::Petted: return "SYNC BOOST";
    case BotMood::Sleeping: return "STANDBY";
  }
  return "PC BOT";
}

const char* BotDisplay::moodHint() const {
  if (!message_.isEmpty()) return message_.c_str();
  switch (mood_) {
    case BotMood::Boot: return "Loading control core";
    case BotMood::Provisioning: return "Open the mobile app";
    case BotMood::Connecting: return "Securing cloud link";
    case BotMood::Happy: return "Hold 2s for PC power";
    case BotMood::Disconnected: return "Check Wi-Fi or MQTT";
    case BotMood::Waking: return "Sending Wake-on-LAN";
    case BotMood::ShuttingDown: return "Graceful command sent";
    case BotMood::SpeedTest: return "Measuring connection";
    case BotMood::Warning: return "Cooling recommended";
    case BotMood::Critical: return "Check PC immediately";
    case BotMood::Resetting: return "Clearing configuration";
    case BotMood::Petted: return "Good input, operator";
    case BotMood::Sleeping: return "Tap to wake display";
  }
  return "";
}

void BotDisplay::drawChrome() {
  display_.fillScreen(kBackground);
  drawHeader();
  drawMetrics();
  drawFooter(millis());
  chromeDirty_ = false;
  metricsDirty_ = false;
}

void BotDisplay::drawHeader() {
  display_.fillRect(0, 0, 320, 37, kBackground);
  display_.fillRect(8, 8, 4, 21, moodAccent());
  display_.setTextColor(kText, kBackground);
  display_.drawString("PC", 19, 5, 4);
  display_.setTextColor(moodAccent(), kBackground);
  display_.drawString("//BOT", 55, 10, 2);

  const char* status = mood_ == BotMood::Happy ? "ONLINE" :
                       mood_ == BotMood::Disconnected ? "OFFLINE" :
                       mood_ == BotMood::Critical ? "ALERT" : "ACTIVE";
  const int16_t width = display_.textWidth(status, 2) + 25;
  const int16_t x = 312 - width;
  display_.fillRoundRect(x, 8, width, 21, 10, kSurface);
  display_.drawRoundRect(x, 8, width, 21, 10, dimColor(moodAccent()));
  display_.fillCircle(x + 11, 18, 3, moodAccent());
  display_.setTextColor(kText, kSurface);
  display_.drawString(status, x + 19, 11, 2);
  display_.drawFastHLine(8, 35, 304, kPanelEdge);
}

void BotDisplay::drawFace(uint32_t now, uint16_t accent) {
  const float phase = static_cast<float>(now % 2200) / 2200.0f * 6.283185f;
  const int16_t bob = mood_ == BotMood::Sleeping ? 1 : static_cast<int16_t>(sinf(phase) * 2.0f);
  const int16_t cx = 66;
  const int16_t cy = 68 + bob;
  const int16_t pulse = 2 + static_cast<int16_t>((sinf(phase) + 1.0f) * 1.5f);

  mainSprite_.drawCircle(cx, cy, 39 + pulse, dimColor(accent));
  mainSprite_.drawCircle(cx, cy, 34, accent);
  mainSprite_.fillRoundRect(cx - 29, cy - 23, 58, 46, 13, kSurface);
  mainSprite_.drawRoundRect(cx - 29, cy - 23, 58, 46, 13, accent);

  const bool blink = mood_ == BotMood::Sleeping || (frame_ % 96) < 5;
  const int16_t eyeY = cy - 5;
  if (blink) {
    mainSprite_.drawFastHLine(cx - 20, eyeY, 13, accent);
    mainSprite_.drawFastHLine(cx + 7, eyeY, 13, accent);
  } else if (mood_ == BotMood::Critical) {
    mainSprite_.drawLine(cx - 21, eyeY - 5, cx - 8, eyeY, accent);
    mainSprite_.drawLine(cx + 8, eyeY, cx + 21, eyeY - 5, accent);
  } else {
    const int16_t gaze = static_cast<int16_t>(sinf(phase * 0.5f) * 2.0f);
    mainSprite_.fillRoundRect(cx - 21, eyeY - 7, 14, 14, 4, accent);
    mainSprite_.fillRoundRect(cx + 7, eyeY - 7, 14, 14, 4, accent);
    mainSprite_.fillCircle(cx - 14 + gaze, eyeY, 2, kBackground);
    mainSprite_.fillCircle(cx + 14 + gaze, eyeY, 2, kBackground);
  }

  if (mood_ == BotMood::Happy || mood_ == BotMood::Petted) {
    mainSprite_.drawLine(cx - 9, cy + 11, cx - 3, cy + 15, accent);
    mainSprite_.drawFastHLine(cx - 3, cy + 15, 6, accent);
    mainSprite_.drawLine(cx + 3, cy + 15, cx + 9, cy + 11, accent);
  } else if (mood_ == BotMood::Warning || mood_ == BotMood::Critical) {
    mainSprite_.drawFastHLine(cx - 9, cy + 14, 18, accent);
  } else {
    mainSprite_.drawCircle(cx, cy + 13, 4, accent);
  }

  if (mood_ == BotMood::Petted) {
    mainSprite_.setTextColor(kMagenta, kBackground);
    mainSprite_.drawString("+XP", 91, 20, 2);
  } else if (mood_ == BotMood::Sleeping) {
    mainSprite_.setTextColor(kCyan, kBackground);
    mainSprite_.drawString("Z", 97, 25, 4);
  }
}

void BotDisplay::drawMain(uint32_t now) {
  if (!spriteReady_) return;
  const uint16_t accent = moodAccent();
  mainSprite_.fillSprite(kBackground);
  mainSprite_.fillRoundRect(0, 0, kMainWidth, kMainHeight, 12, kSurface);
  mainSprite_.drawRoundRect(0, 0, kMainWidth, kMainHeight, 12, kPanelEdge);
  mainSprite_.drawFastVLine(132, 13, 100, kPanelEdge);

  for (int16_t y = 15; y < kMainHeight - 8; y += 12) {
    mainSprite_.drawFastHLine(9, y, kMainWidth - 18, dimColor(kPanelEdge));
  }

  drawFace(now, accent);

  mainSprite_.fillRoundRect(151, 15, 137, 21, 10, dimColor(kPanel));
  mainSprite_.fillCircle(163, 25, 3, accent);
  mainSprite_.setTextColor(accent, dimColor(kPanel));
  mainSprite_.drawString(moodLabel(), 172, 18, 2);

  if (mood_ == BotMood::Provisioning && !setupSsid_.isEmpty()) {
    mainSprite_.setTextColor(kMuted, kSurface);
    mainSprite_.drawString("WI-FI HOTSPOT", 151, 44, 1);
    mainSprite_.setTextColor(kText, kSurface);
    mainSprite_.drawString(setupSsid_, 151, 54, 2);
    mainSprite_.setTextColor(kMuted, kSurface);
    mainSprite_.drawString("PASSWORD", 151, 75, 1);
    mainSprite_.setTextColor(kGreen, kSurface);
    mainSprite_.drawString(setupPassword_, 151, 85, 2);
    mainSprite_.setTextColor(kCyan, kSurface);
    mainSprite_.drawString(setupUrl_, 151, 107, 1);
    mainSprite_.pushSprite(8, 40);
    return;
  }

  mainSprite_.setTextColor(kText, kSurface);
  mainSprite_.drawString("COMMAND CORE", 151, 48, 2);
  mainSprite_.setTextColor(kMuted, kSurface);
  String hint = moodHint();
  if (hint.length() > 21) hint = hint.substring(0, 21);
  mainSprite_.drawString(hint, 151, 67, 2);

  const int16_t progress = mood_ == BotMood::Happy
                               ? 126
                               : 24 + static_cast<int16_t>((now / 16) % 102);
  mainSprite_.fillRoundRect(151, 96, 137, 7, 3, kPanel);
  mainSprite_.fillRoundRect(151, 96, progress, 7, 3, accent);
  mainSprite_.setTextColor(kMuted, kSurface);
  mainSprite_.drawString(mood_ == BotMood::Happy ? "LINK STABLE" : "PROCESSING",
                         151, 108, 1);

  mainSprite_.pushSprite(8, 40);
}

void BotDisplay::drawMetricCard(int16_t x, const char* label, float value,
                                const char* unit, uint16_t accent) {
  display_.fillRoundRect(x, 171, 96, 52, 8, kSurface);
  display_.drawRoundRect(x, 171, 96, 52, 8, kPanelEdge);
  display_.fillRect(x + 8, 179, 3, 12, accent);
  display_.setTextColor(kMuted, kSurface);
  display_.drawString(label, x + 16, 176, 2);

  char valueText[12];
  if (isnan(value)) {
    snprintf(valueText, sizeof(valueText), "--");
  } else {
    snprintf(valueText, sizeof(valueText), "%.0f", value);
  }
  display_.setTextColor(kText, kSurface);
  display_.drawString(valueText, x + 9, 196, 4);
  const int16_t valueWidth = display_.textWidth(valueText, 4);
  display_.setTextColor(accent, kSurface);
  display_.drawString(unit, x + 12 + valueWidth, 204, 2);
}

void BotDisplay::drawMetrics() {
  drawMetricCard(8, "CPU TEMP", cpuTemp_, "C", kCyan);
  drawMetricCard(112, "GPU TEMP", gpuTemp_, "C", kMagenta);
  drawMetricCard(216, "MEMORY", memoryPercent_, "%", kGreen);
  metricsDirty_ = false;
}

void BotDisplay::drawFooter(uint32_t now) {
  display_.fillRect(0, 226, 320, 14, kBackground);
  const int16_t sweep = static_cast<int16_t>((now / 18) % 320);
  const int16_t sweepStart = sweep > 25 ? sweep - 25 : 0;
  const int16_t sweepWidth = sweep < 25 ? sweep : 25;
  display_.drawFastHLine(0, 226, 320, kPanelEdge);
  if (sweepWidth > 0) {
    display_.drawFastHLine(sweepStart, 226, sweepWidth, moodAccent());
  }
  display_.setTextColor(kMuted, kBackground);
  display_.drawString("TAP: INTERACT", 8, 230, 1);
  display_.setTextDatum(TR_DATUM);
  display_.drawString("HOLD: POWER", 312, 230, 1);
  display_.setTextDatum(TL_DATUM);
}

void BotDisplay::tick(uint32_t now) {
  if (now - lastFrame_ < BoardConfig::kDisplayFrameMs) return;
  lastFrame_ = now;
  frame_++;

  if (now - lastActivity_ > BoardConfig::kIdleSleepMs &&
      mood_ != BotMood::Sleeping && mood_ != BotMood::Warning &&
      mood_ != BotMood::Critical && mood_ != BotMood::Provisioning &&
      mood_ != BotMood::Disconnected) {
    mood_ = BotMood::Sleeping;
    message_ = "";
    transitionStarted_ = now;
    chromeDirty_ = true;
  }

  if (chromeDirty_) {
    drawHeader();
    metricsDirty_ = true;
    chromeDirty_ = false;
  }
  drawMain(now);
  if (metricsDirty_) drawMetrics();
  drawFooter(now);
}
