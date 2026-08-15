#include "BotDisplay.h"

#include <math.h>
#include <time.h>

#include "BoardConfig.h"
#include "DisplayTheme.h"
#include "Icons.h"
#include "LiquidBar.h"

namespace {
constexpr int16_t kScreenWidth = 240;
constexpr int16_t kScreenHeight = 320;
constexpr int16_t kMargin = 8;

constexpr int16_t kHeaderHeight = 32;

constexpr int16_t kTileY = 36;
constexpr int16_t kTileWidth = 224;
constexpr int16_t kTileHeight = 96;

constexpr int16_t kCardWidth = 224;
constexpr int16_t kCardHeight = 50;
constexpr int16_t kCardFirstY = 138;
constexpr int16_t kCardSpacing = 54;
// The volume overlay borrows the metric card sprite and lands over the middle
// card, which is the least useful thing to cover for a moment.
constexpr int16_t kVolumeOverlayY = 192;

constexpr int16_t kFooterY = 300;

constexpr int16_t kWelcomeWidth = 224;
constexpr int16_t kWelcomeHeight = 160;
constexpr int16_t kWelcomeX = 8;
constexpr int16_t kWelcomeY = 76;

constexpr float kSleepDimTarget = 0.62f;
constexpr float kWarnRatio = 0.75f;
constexpr float kCriticalRatio = 0.88f;

// Thermal gauge scale. Silicon idles near 30 C and throttles around 90 C, so
// the dial spans that band instead of 0-100 where nothing ever moves.
constexpr float kTempFloorC = 30.0f;
constexpr float kTempCeilingC = 95.0f;
constexpr float kTempWarnC = 80.0f;
constexpr float kTempCriticalC = 90.0f;
constexpr uint8_t kNoSample = 255;

float approach(float current, float target, float step) {
  if (current < target)
    return current + step > target ? target : current + step;
  if (current > target)
    return current - step < target ? target : current - step;
  return current;
}

float ratioOf(float value) {
  if (isnan(value))
    return 0.0f;
  return theme::clamp01(value / 100.0f);
}

uint16_t statusColor(float ratio, uint16_t calm) {
  if (ratio >= kCriticalRatio)
    return theme::kDanger;
  if (ratio >= kWarnRatio)
    return theme::kWarning;
  return calm;
}

// Position of a temperature on the dial, 0..1.
float tempRatio(float celsius) {
  if (isnan(celsius))
    return 0.0f;
  return theme::clamp01((celsius - kTempFloorC) /
                        (kTempCeilingC - kTempFloorC));
}

uint16_t tempColor(float celsius, uint16_t calm) {
  if (isnan(celsius))
    return theme::kMuted;
  if (celsius >= kTempCriticalC)
    return theme::kDanger;
  if (celsius >= kTempWarnC)
    return theme::kWarning;
  return calm;
}

// History is stored in quarter-Mb steps so a byte covers 0-63.75 Mb with
// useful resolution; 254 saturates, 255 marks a gap.
uint8_t toQuarterMb(float mbps) {
  if (isnan(mbps) || mbps < 0.0f)
    return kNoSample;
  const float quarters = mbps * 4.0f;
  return quarters > 254.0f ? 254 : static_cast<uint8_t>(quarters + 0.5f);
}

void valueText(char *out, size_t size, float value, const char *unit,
               bool unitPrefix = true) {
  if (isnan(value)) {
    snprintf(out, size, "--");
  } else if (unitPrefix) {
    snprintf(out, size, "%s%.0f", unit, value);
  } else {
    snprintf(out, size, "%.0f%s", value, unit);
  }
}
} // namespace

BotDisplay::BotDisplay()
    : welcomeCanvas_(&display_),
      net_(&display_),
      card_(&display_),
      footer_(&display_) {
  for (uint8_t i = 0; i < kHistorySamples; ++i) {
    downHistory_[i] = kNoSample;
    upHistory_[i] = kNoSample;
  }
}

void BotDisplay::begin() {
  display_.init();
  display_.setRotation(0); // Portrait, 240x320.
  display_.setTextDatum(TL_DATUM);
  display_.fillScreen(theme::kInk);

  // The intro owns a large canvas that is released before the dashboard
  // sprites are allocated, so the two never occupy memory at the same time.
  welcomeCanvas_.setColorDepth(16);
  if (welcomeCanvas_.createSprite(kWelcomeWidth, kWelcomeHeight) == nullptr) {
    display_.setTextColor(theme::kDanger, theme::kInk);
    display_.drawCentreString("DISPLAY BUFFER ERROR", 120, 140, 4);
    display_.setTextColor(theme::kMuted, theme::kInk);
    display_.drawCentreString("Restart the controller", 120, 170, 2);
    return;
  }
  welcomeCanvas_.setTextDatum(TL_DATUM);

  const uint32_t now = millis();
  lastActivity_ = now;
  moodStarted_ = now;
  welcome_.begin(display_, now);
}

void BotDisplay::setMood(BotMood mood, const String &message) {
  if (mood_ == mood && message_ == message)
    return;
  mood_ = mood;
  message_ = message;
  const uint32_t now = millis();
  moodStarted_ = now;
  if (mood != BotMood::Petted && mood != BotMood::Sleeping)
    baseMood_ = mood;
  chromeDirty_ = true;
  provisioningDirty_ = true;
  noteActivity(now);
}

void BotDisplay::setMetrics(float cpuTemp, float gpuTemp, float memoryPercent) {
  cpu_.target = cpuTemp;
  gpu_.target = gpuTemp;
  memory_.target = memoryPercent;
  // Seed the animation on the first reading so values do not sweep up from
  // zero every time the PC reconnects.
  if (isnan(cpu_.shown))
    cpu_.shown = cpuTemp;
  if (isnan(gpu_.shown))
    gpu_.shown = gpuTemp;
  if (isnan(memory_.shown))
    memory_.shown = memoryPercent;
}

void BotDisplay::setNetwork(float downloadMbps, float uploadMbps,
                            int16_t rssiDbm) {
  rssiDbm_ = rssiDbm;
  // Throughput is optional: a caller may refresh only the link strength.
  if (isnan(downloadMbps) && isnan(uploadMbps))
    return;

  if (!isnan(downloadMbps)) {
    download_.target = downloadMbps;
    if (isnan(download_.shown))
      download_.shown = downloadMbps;
  }
  if (!isnan(uploadMbps)) {
    upload_.target = uploadMbps;
    if (isnan(upload_.shown))
      upload_.shown = uploadMbps;
  }

  // Track a slowly decaying peak so the trend scales to whatever this
  // connection actually does, rather than a hardcoded maximum.
  const float down = isnan(downloadMbps) ? 0.0f : downloadMbps;
  const float up = isnan(uploadMbps) ? 0.0f : uploadMbps;
  const float busiest = down > up ? down : up;
  throughputPeak_ *= 0.97f;
  if (busiest > throughputPeak_)
    throughputPeak_ = busiest;
  if (throughputPeak_ < 1.0f)
    throughputPeak_ = 1.0f;

  downHistory_[historyHead_] = toQuarterMb(downloadMbps);
  upHistory_[historyHead_] = toQuarterMb(uploadMbps);
  historyHead_ = (historyHead_ + 1) % kHistorySamples;
  if (historyCount_ < kHistorySamples)
    historyCount_++;
}

void BotDisplay::setMedia(const String& status, const String& title,
                          const String& artist, const String& app,
                          float positionSeconds, float durationSeconds) {
  const bool playing = status == "playing";
  // Starting, stopping or changing track swaps what the summary slot shows, so
  // it is worth waking the screen for; a position update is not.
  const bool changed = playing != mediaPlaying_ || title != mediaTitle_;
  mediaPlaying_ = playing;
  mediaTitle_ = title;
  mediaArtist_ = artist;
  mediaApp_ = app;
  mediaPosition_ = positionSeconds;
  mediaDuration_ = durationSeconds;
  mediaStamp_ = millis();
  if (changed) noteActivity(mediaStamp_);
}

void BotDisplay::setVolume(uint8_t level, bool muted) {
  const bool changed = !volumeKnown_ || level != volumeLevel_ || muted != volumeMuted_;
  volumeLevel_ = level;
  volumeMuted_ = muted;
  volumeKnown_ = true;
  if (!changed) return;
  const uint32_t now = millis();
  volumeOverlayUntil_ = now + BoardConfig::kVolumeOverlayMs;
  noteActivity(now);
}

void BotDisplay::setUpdateProgress(uint8_t percent) {
  const bool first = !updating_;
  updating_ = true;
  updatePercent_ = percent > 100 ? 100 : percent;
  if (first) display_.fillScreen(theme::kInk);

  // Drawn straight from the setter: the upload handler owns the CPU while the
  // flash is being written, so tick() does not run during a transfer.
  const uint16_t background = theme::kInk;
  const int16_t cardX = 16;
  const int16_t cardY = 108;
  const int16_t cardW = kScreenWidth - cardX * 2;
  const int16_t cardH = 104;

  theme::glassCard(display_, cardX, cardY, cardW, cardH, 14, background,
                   theme::kPrimary, 0.45f);
  display_.setTextDatum(TC_DATUM);
  display_.setTextColor(theme::kTextSoft);
  display_.drawString("UPDATING FIRMWARE", kScreenWidth / 2, cardY + 16, 2);

  char text[8];
  snprintf(text, sizeof(text), "%u%%", updatePercent_);
  display_.setTextColor(theme::kText);
  display_.drawString(text, kScreenWidth / 2, cardY + 38, 4);
  display_.setTextDatum(TL_DATUM);

  liquid::bar(display_, cardX + 14, cardY + cardH - 26, cardW - 28, 12,
              updatePercent_ / 100.0f, millis(), theme::kPrimary,
              theme::kTertiary, background);

  display_.setTextDatum(TC_DATUM);
  display_.setTextColor(theme::kMuted);
  display_.drawString("Do not unplug the device", kScreenWidth / 2, cardY + cardH + 12, 1);
  display_.setTextDatum(TL_DATUM);
}

float BotDisplay::mediaPositionAt(uint32_t now) const {
  if (isnan(mediaPosition_)) return NAN;
  if (!mediaPlaying_) return mediaPosition_;
  // The PC only reports every second or so; run the clock forward between
  // updates so the progress bar moves smoothly.
  const float advanced = mediaPosition_ + (now - mediaStamp_) / 1000.0f;
  if (!isnan(mediaDuration_) && advanced > mediaDuration_) return mediaDuration_;
  return advanced;
}

uint8_t BotDisplay::signalBars() const {
  if (rssiDbm_ == 0)
    return 0;
  if (rssiDbm_ >= -60)
    return 3;
  if (rssiDbm_ >= -70)
    return 2;
  if (rssiDbm_ >= -80)
    return 1;
  return 0;
}

void BotDisplay::setProvisioningInfo(const String &ssid, const String &password,
                                     const String &url) {
  setupSsid_ = ssid;
  setupPassword_ = password;
  setupUrl_ = url;
  chromeDirty_ = true;
  provisioningDirty_ = true;
}

void BotDisplay::setTouch(bool pressed, float holdProgress) {
  if (pressed && !touchPressed_) {
    // Skip the rest of the intro as soon as the operator touches the pad.
    if (welcome_.active())
      finishIntro();
    pressGlow_ = 1.0f;
  }
  touchPressed_ = pressed;
  holdProgress_ = theme::clamp01(holdProgress);
  if (pressed)
    noteActivity(millis());
}

void BotDisplay::setPcOnline(bool online) {
  const bool changed = !pcStateKnown_ || pcOnline_ != online;
  pcOnline_ = online;
  pcStateKnown_ = true;
  if (changed)
    noteActivity(millis());
}

void BotDisplay::noteActivity(uint32_t now) {
  lastActivity_ = now;
  if (mood_ == BotMood::Sleeping) {
    mood_ = preSleepMood_;
    moodStarted_ = now;
    chromeDirty_ = true;
  }
}

uint16_t BotDisplay::shade(uint16_t color) const { return theme::tone(color); }

uint16_t BotDisplay::moodAccent() const {
  switch (mood_) {
  case BotMood::Warning:
    return theme::kWarning;
  case BotMood::Critical:
    return theme::kDanger;
  case BotMood::Disconnected:
    return theme::kMuted;
  case BotMood::Happy:
    return theme::kSuccess;
  default:
    return theme::kPrimary;
  }
}

const char *BotDisplay::moodLabel() const {
  switch (mood_) {
  case BotMood::Boot:
    return "STARTING UP";
  case BotMood::Provisioning:
    return "SETUP MODE";
  case BotMood::Connecting:
    return "CONNECTING";
  case BotMood::Happy:
    return "ALL GOOD";
  case BotMood::Disconnected:
    return "OFFLINE";
  case BotMood::Waking:
    return "WAKING PC";
  case BotMood::ShuttingDown:
    return "SHUTTING DOWN";
  case BotMood::SpeedTest:
    return "SPEED TEST";
  case BotMood::Warning:
    return "RUNNING HOT";
  case BotMood::Critical:
    return "NEEDS YOU";
  case BotMood::Resetting:
    return "RESETTING";
  case BotMood::Petted:
    return "HELLO!";
  case BotMood::Sleeping:
    return "RESTING";
  }
  return BoardConfig::kProductFullName;
}

const char *BotDisplay::moodHint() const {
  if (!message_.isEmpty())
    return message_.c_str();
  switch (mood_) {
  case BotMood::Boot:
    return "Waking up";
  case BotMood::Provisioning:
    return "Finish setup in the app";
  case BotMood::Connecting:
    return "Securing the link";
  case BotMood::Happy:
    return "Hold 2s for PC power";
  case BotMood::Disconnected:
    return "Check Wi-Fi or broker";
  case BotMood::Waking:
    return "Wake-on-LAN sent";
  case BotMood::ShuttingDown:
    return "Shutdown sent";
  case BotMood::SpeedTest:
    return "Measuring link";
  case BotMood::Warning:
    return "Give the PC air";
  case BotMood::Critical:
    return "Check the PC now";
  case BotMood::Resetting:
    return "Clearing config";
  case BotMood::Petted:
    return "Nice to see you";
  case BotMood::Sleeping:
    return "Tap to wake";
  }
  return "";
}

void BotDisplay::updateValues() {
  // Values glide towards their target so every change is animated rather than
  // snapping between readings.
  if (!isnan(cpu_.target))
    cpu_.shown = approach(cpu_.shown, cpu_.target, 0.9f);
  if (!isnan(gpu_.target))
    gpu_.shown = approach(gpu_.shown, gpu_.target, 0.9f);
  if (!isnan(memory_.target)) {
    memory_.shown = approach(memory_.shown, memory_.target, 1.4f);
  }
  if (!isnan(download_.target)) {
    download_.shown =
        approach(download_.shown, download_.target, throughputPeak_ * 0.04f);
  }
  if (!isnan(upload_.target)) {
    upload_.shown =
        approach(upload_.shown, upload_.target, throughputPeak_ * 0.04f);
  }
}

void BotDisplay::drawHeader(uint32_t now) {
  const uint16_t accent = shade(moodAccent());
  theme::verticalGradient(display_, 0, 0, kScreenWidth, kHeaderHeight,
                          shade(theme::kBackdrop), shade(theme::kInk));

  theme::embossedText(display_, BoardConfig::kProductName, kMargin, 2, 4,
                      shade(theme::kText), shade(theme::kBevelDark));
  display_.setTextColor(shade(theme::kMuted));
  display_.drawString(
      BoardConfig::kProductSuffix,
      kMargin + 6 + display_.textWidth(BoardConfig::kProductName, 4), 11, 1);

  const char *status = mood_ == BotMood::Happy          ? "ONLINE"
                       : mood_ == BotMood::Disconnected ? "OFFLINE"
                       : mood_ == BotMood::Critical     ? "ALERT"
                       : mood_ == BotMood::Sleeping     ? "STANDBY"
                                                        : "BUSY";
  const int16_t pillWidth = display_.textWidth(status, 1) + 34;
  const int16_t pillX = kScreenWidth - kMargin - pillWidth;
  theme::gradientPill(
      display_, pillX, 6, pillWidth, 20,
      shade(mood_ == BotMood::Happy ? theme::kSuccess : theme::kPrimary),
      shade(mood_ == BotMood::Happy ? theme::kTertiary : theme::kSecondary));
  icons::link(display_, pillX + 6, 9, 13, shade(theme::kWhite),
              theme::mix(accent, theme::kInk, 0.55f),
              mood_ == BotMood::Disconnected ? 1 : 3);
  display_.setTextColor(shade(theme::kWhite));
  display_.drawString(status, pillX + 23, 11, 1);

  display_.drawFastHLine(0, kHeaderHeight - 1, kScreenWidth,
                         theme::mix(shade(theme::kInk), accent, 0.30f));
  (void)now;
}

void BotDisplay::drawTrace(const uint8_t* samples, int16_t plotX,
                           int16_t plotY, int16_t plotW, int16_t plotH,
                           uint16_t color, bool filled, uint32_t now) {
  const uint16_t background = shade(theme::kInk);
  if (historyCount_ < 2) return;

  // Pre-compute pixel-space positions for all valid (non-gap) samples.
  float pxArr[kHistorySamples];
  float pyArr[kHistorySamples];
  uint8_t n = 0;
  for (uint8_t step = 0; step < historyCount_; ++step) {
    const uint8_t slot =
        (historyHead_ + kHistorySamples - historyCount_ + step) %
        kHistorySamples;
    const uint8_t sample = samples[slot];
    if (sample == kNoSample) continue;
    const float ratio = theme::clamp01((sample / 4.0f) / throughputPeak_);
    pxArr[n] = plotX + static_cast<float>(plotW) * step / (kHistorySamples - 1);
    pyArr[n] = plotY + plotH - ratio * plotH;
    ++n;
  }
  if (n < 2) return;

  // Pre-compute 3 fill-band colors once — avoids repeated mix() per pixel.
  const uint16_t fillTop = theme::mix(background, color, 0.52f);
  const uint16_t fillMid = theme::mix(fillTop, background, 0.45f);
  const uint16_t fillBot = theme::mix(fillMid, background, 0.70f);

  // Catmull-Rom spline — 3 sub-steps at 4.5 px/sample ≈ 1.5 px/step.
  // Smooth enough on this display; 40% fewer evaluations than kSub=5.
  constexpr int kSub = 3;
  int16_t prevX = -1, prevY = -1;

  for (uint8_t i = 0; i + 1 < n; ++i) {
    const uint8_t i0 = (i > 0) ? i - 1 : 0;
    const uint8_t i3 = (i + 2 < n) ? i + 2 : i + 1;
    const float x0 = pxArr[i0], y0 = pyArr[i0];
    const float x1 = pxArr[i],   y1 = pyArr[i];
    const float x2 = pxArr[i+1], y2 = pyArr[i+1];
    const float x3 = pxArr[i3],  y3 = pyArr[i3];

    for (int s = (i == 0 ? 0 : 1); s <= kSub; ++s) {
      const float t  = s / static_cast<float>(kSub);
      const float t2 = t * t;
      const float t3 = t2 * t;
      const int16_t cx = static_cast<int16_t>(0.5f * (
          2*x1 + (-x0+x2)*t + (2*x0-5*x1+4*x2-x3)*t2 + (-x0+3*x1-3*x2+x3)*t3));
      int16_t cy = static_cast<int16_t>(0.5f * (
          2*y1 + (-y0+y2)*t + (2*y0-5*y1+4*y2-y3)*t2 + (-y0+3*y1-3*y2+y3)*t3));
      // Clamp to plot bounds.
      if (cy < plotY)          cy = plotY;
      if (cy > plotY + plotH)  cy = plotY + plotH;

      if (filled) {
        // 3-band VLine fill — replaces N×drawPixel+floatMath with 3 fast blits.
        const int16_t fillH = plotY + plotH - cy;
        if (fillH > 0) {
          const int16_t h1 = fillH / 3;
          const int16_t h2 = fillH / 3;
          const int16_t h3 = fillH - h1 - h2;
          if (h1 > 0) net_.drawFastVLine(cx, cy,           h1, fillTop);
          if (h2 > 0) net_.drawFastVLine(cx, cy + h1,      h2, fillMid);
          if (h3 > 0) net_.drawFastVLine(cx, cy + h1 + h2, h3, fillBot);
        }
      }
      if (prevX >= 0) net_.drawLine(prevX, prevY, cx, cy, color);
      prevX = cx;
      prevY = cy;
    }
  }

  // Breathing endpoint dot.
  if (prevX >= 0) {
    const float breathe =
        (sinf(static_cast<float>(now % 1600) / 1600.0f * 6.2831853f) + 1.0f) *
        0.5f;
    net_.fillCircle(prevX, prevY, 3,
                    theme::mix(background, color, 0.35f + breathe * 0.45f));
    net_.fillCircle(prevX, prevY, 1, shade(theme::kWhite));
  }
}

void BotDisplay::drawNetwork(uint32_t now) {
  const uint16_t background = shade(theme::kInk);
  net_.fillSprite(background);
  theme::glassCard(net_, 0, 0, kTileWidth, kTileHeight, 14, background,
                   shade(theme::kTertiary), 0.35f);

  // ── Header row ─────────────────────────────────────────────────────────
  const uint8_t  bars     = signalBars();
  const uint16_t linkTone = bars == 0 ? shade(theme::kDanger)
                          : bars == 1 ? shade(theme::kWarning)
                                      : shade(theme::kTertiary);

  icons::link(net_, 10, 6, 14, linkTone,
              theme::mix(background, linkTone, 0.25f), bars);
  net_.setTextColor(shade(theme::kTextSoft));
  net_.drawString("NETWORK ACTIVITY", 30, 8, 1);

  char text[20];
  net_.setTextDatum(TR_DATUM);
  net_.setTextColor(linkTone);
  if (rssiDbm_ == 0) {
    net_.drawString("NO LINK", kTileWidth - 10, 8, 1);
  } else {
    snprintf(text, sizeof(text), "%d dBm", rssiDbm_);
    net_.drawString(text, kTileWidth - 10, 8, 1);
  }
  net_.setTextDatum(TL_DATUM);

  // ── Two-panel layout ────────────────────────────────────────────────────
  // Each panel: label+value row (8 px) + plot area (25 px).
  // Total: header(20) + sep(1) + DOWN(34) + sep(1) + UP(34) + pad(6) = 96
  const int16_t plotX  = 10;
  const int16_t plotW  = kTileWidth - 20;
  const int16_t sepY   = 20;   // below header
  const int16_t midY   = 55;   // between DOWN and UP panels

  const int16_t dLabelY = 22;
  const int16_t dPlotY  = 30;
  const int16_t dPlotH  = midY - dPlotY - 1;  // 24 px

  const int16_t uLabelY = 57;
  const int16_t uPlotY  = 65;
  const int16_t uPlotH  = kTileHeight - uPlotY - 5;  // 26 px

  const uint16_t downColor = shade(theme::kTertiary);
  const uint16_t upColor   = shade(theme::kSecondary);

  // Divider lines.
  const uint16_t divColor = theme::mix(background, theme::kBevelLight, 0.18f);
  net_.drawFastHLine(8, sepY, kTileWidth - 16, divColor);
  net_.drawFastHLine(8, midY, kTileWidth - 16, divColor);

  // Subtle grid lines inside each panel.
  const uint16_t gridColor = theme::mix(background, theme::kBevelLight, 0.09f);
  for (uint8_t g = 1; g < 3; ++g) {
    net_.drawFastHLine(plotX, dPlotY + dPlotH * g / 3, plotW, gridColor);
    net_.drawFastHLine(plotX, uPlotY + uPlotH * g / 3, plotW, gridColor);
  }

  // ── DOWN panel label + live reading ─────────────────────────────────────
  net_.setTextColor(shade(theme::kMuted));
  net_.drawString("DOWN", plotX, dLabelY, 1);
  net_.setTextDatum(TR_DATUM);
  net_.setTextColor(downColor);
  if (isnan(download_.shown)) {
    net_.drawString("--", kTileWidth - 10, dLabelY, 1);
  } else {
    snprintf(text, sizeof(text), "%.1f Mb/s", download_.shown);
    net_.drawString(text, kTileWidth - 10, dLabelY, 1);
  }
  net_.setTextDatum(TL_DATUM);

  // ── UP panel label + live reading ───────────────────────────────────────
  net_.setTextColor(shade(theme::kMuted));
  net_.drawString("UP", plotX, uLabelY, 1);
  net_.setTextDatum(TR_DATUM);
  net_.setTextColor(upColor);
  if (isnan(upload_.shown)) {
    net_.drawString("--", kTileWidth - 10, uLabelY, 1);
  } else {
    snprintf(text, sizeof(text), "%.1f Mb/s", upload_.shown);
    net_.drawString(text, kTileWidth - 10, uLabelY, 1);
  }
  net_.setTextDatum(TL_DATUM);

  // ── Graphs ───────────────────────────────────────────────────────────────
  if (historyCount_ == 0) {
    net_.setTextDatum(MC_DATUM);
    net_.setTextColor(shade(theme::kMuted));
    net_.drawString("WAITING FOR TELEMETRY", kTileWidth / 2,
                    dPlotY + dPlotH / 2, 1);
    net_.setTextDatum(TL_DATUM);
  } else {
    drawTrace(downHistory_, plotX, dPlotY, plotW, dPlotH, downColor, true, now);
    drawTrace(upHistory_,   plotX, uPlotY, plotW, uPlotH, upColor,   true, now);
  }

  theme::sheenSweep(net_, 1, 1, kTileWidth - 2, kTileHeight - 2,
                    static_cast<float>(now % 7000) / 7000.0f * 1.4f - 0.2f,
                    0.10f);
  net_.pushSprite(kMargin, kTileY);
}

void BotDisplay::drawMedia(uint32_t now) {
  const uint16_t background = shade(theme::kInk);
  const uint16_t accent = shade(mediaPlaying_ ? theme::kSuccess : theme::kMuted);
  net_.fillSprite(background);
  theme::glassCard(net_, 0, 0, kTileWidth, kTileHeight, 14, background, accent,
                   0.35f);

  icons::note(net_, 10, 6, 13, shade(theme::kText), accent);
  net_.setTextColor(shade(theme::kTextSoft));
  net_.drawString(mediaApp_.isEmpty() ? "NOW PLAYING" : mediaApp_, 30, 8, 1);

  icons::transport(net_, kTileWidth - 20, 6, 11, accent, mediaPlaying_);
  net_.setTextDatum(TR_DATUM);
  net_.setTextColor(accent);
  net_.drawString(mediaPlaying_ ? "PLAYING" : "PAUSED", kTileWidth - 26, 8, 1);
  net_.setTextDatum(TL_DATUM);

  // Title, scrolled when it does not fit. The gap keeps the loop readable
  // instead of running the end into the beginning.
  const int16_t inset = 12;
  const int16_t room = kTileWidth - inset * 2;
  const int16_t titleWidth = net_.textWidth(mediaTitle_, 4);
  net_.setTextColor(shade(theme::kText));
  if (titleWidth <= room) {
    net_.drawString(mediaTitle_, inset, 24, 4);
  } else {
    constexpr int16_t kGap = 28;
    const int16_t span = titleWidth + kGap;
    const int16_t offset = static_cast<int16_t>((now / 32) % span);
    // Clipped so the scroll cannot spill past the rounded edge.
    net_.setViewport(inset, 24, room, 26);
    net_.drawString(mediaTitle_, -offset, 0, 4);
    net_.drawString(mediaTitle_, -offset + span, 0, 4);
    net_.resetViewport();
  }

  net_.setTextColor(shade(theme::kTextSoft));
  net_.drawString(mediaArtist_, inset, 52, 2);

  const float position = mediaPositionAt(now);
  const bool timed =
      !isnan(position) && !isnan(mediaDuration_) && mediaDuration_ > 0.0f;
  char text[12];
  net_.setTextColor(shade(theme::kMuted));
  if (timed) {
    const int elapsed = static_cast<int>(position);
    snprintf(text, sizeof(text), "%d:%02d", elapsed / 60, elapsed % 60);
    net_.drawString(text, inset, 70, 1);
    const int total = static_cast<int>(mediaDuration_);
    snprintf(text, sizeof(text), "%d:%02d", total / 60, total % 60);
    net_.setTextDatum(TR_DATUM);
    net_.drawString(text, kTileWidth - inset, 70, 1);
    net_.setTextDatum(TL_DATUM);
  } else {
    net_.drawString("LIVE", inset, 70, 1);
  }

  liquid::bar(net_, inset, 80, room, 10,
              timed ? theme::clamp01(position / mediaDuration_) : 1.0f, now,
              shade(theme::kPrimary), shade(theme::kSecondary), background);

  theme::sheenSweep(net_, 1, 1, kTileWidth - 2, kTileHeight - 2,
                    static_cast<float>(now % 7000) / 7000.0f * 1.4f - 0.2f,
                    0.10f);
  net_.pushSprite(kMargin, kTileY);
}

void BotDisplay::drawVolumeOverlay(uint32_t now) {
  const uint16_t background = shade(theme::kInk);
  const uint16_t accent =
      shade(volumeMuted_ || volumeLevel_ == 0 ? theme::kMuted : theme::kTertiary);
  const uint8_t waves = volumeLevel_ >= 66 ? 3 : (volumeLevel_ >= 33 ? 2 : 1);

  card_.fillSprite(background);
  theme::glow(card_, 2, 2, kCardWidth - 4, kCardHeight - 4, 14, accent,
              background, 3);
  theme::glassCard(card_, 0, 0, kCardWidth, kCardHeight, 14, background, accent,
                   0.55f);

  icons::speaker(card_, 12, 15, 20, shade(theme::kText),
                 theme::mix(background, accent, 0.30f), waves,
                 volumeMuted_ || volumeLevel_ == 0);

  card_.setTextColor(shade(theme::kMuted));
  card_.drawString("PC VOLUME", 46, 8, 1);

  char text[8];
  snprintf(text, sizeof(text), "%u", volumeLevel_);
  card_.setTextDatum(TR_DATUM);
  card_.setTextColor(shade(theme::kText));
  card_.drawString(text, kCardWidth - 24, 4, 4);
  card_.setTextDatum(TL_DATUM);
  card_.setTextColor(accent);
  card_.drawString("%", kCardWidth - 20, 16, 2);

  liquid::bar(card_, 46, 28, kCardWidth - 76, 12,
              volumeMuted_ ? 0.0f : volumeLevel_ / 100.0f, now,
              shade(theme::kPrimary), accent, background);

  card_.pushSprite(kMargin, kVolumeOverlayY);
}

void BotDisplay::drawMetricCard(uint8_t index, int16_t y, const char *label,
                                const Metric &metric, const char *unit,
                                uint16_t calm, bool temperature, uint32_t now,
                                bool unitPrefix) {
  const uint16_t background = shade(theme::kInk);
  // Temperatures use the thermal dial's scale so the card and the gauge above
  // it never disagree about what "hot" means.
  const float ratio =
      temperature ? tempRatio(metric.shown) : ratioOf(metric.shown);
  const uint16_t tone = shade(temperature ? tempColor(metric.shown, calm)
                                          : statusColor(ratio, calm));

  card_.fillSprite(background);
  theme::glassCard(card_, 0, 0, kCardWidth, kCardHeight, 14, background, tone,
                   0.30f);

  // Icon tile carries the component's own accent, which is what links each row
  // to its arc on the thermal dial.
  theme::gradientPill(card_, 8, 10, 32, 32, shade(calm),
                      theme::mix(shade(calm), shade(theme::kTertiary), 0.55f));
  const uint16_t glyph = shade(theme::kWhite);
  if (index == 0) {
    icons::cpu(card_, 16, 18, 16, glyph, shade(theme::kTertiary));
  } else if (index == 1) {
    icons::gpu(card_, 16, 18, 16, glyph, shade(theme::kTertiary));
  } else {
    icons::memory(card_, 16, 18, 16, glyph, shade(theme::kTertiary));
  }

  // Label and value share the top line; the bar owns the bottom one. Font 4 is
  // 26 px tall, so the value must clear y=30 before the bar starts.
  card_.setTextColor(shade(theme::kTextSoft));
  card_.drawString(label, 48, 11, 1);

  char numStr[8];
  card_.setTextColor(tone);
  if (isnan(metric.shown)) {
    card_.setTextDatum(TR_DATUM);
    card_.drawString("--", kCardWidth - 12, 4, 4);
    card_.setTextDatum(TL_DATUM);
  } else if (temperature) {
    // Draw <number> + hand-drawn ° circle + C, right-aligned.
    // Built-in TFT_eSPI fonts don't include the degree glyph so we
    // paint a small circle ourselves between the digits and the "C".
    snprintf(numStr, sizeof(numStr), "%.0f", metric.shown);
    const int16_t numW   = card_.textWidth(numStr, 4);
    const int16_t cW     = card_.textWidth("C", 4);
    const int16_t totalW = numW + 10 + cW;  // 10 = circle gap
    const int16_t startX = kCardWidth - 12 - totalW;
    card_.setTextDatum(TL_DATUM);
    card_.drawString(numStr, startX, 4, 4);
    card_.drawCircle(startX + numW + 5, 7, 3, tone);  // ° dot
    card_.drawString("C", startX + numW + 10, 4, 4);
  } else if (unitPrefix) {
    // Draw large number in font 4 + small "%" in font 2, right-aligned.
    // Font 4 is 26 px tall (y=4..30), font 2 is 16 px tall.
    // Align baselines: y_pct = 30 - 16 = 14.
    snprintf(numStr, sizeof(numStr), "%.0f", metric.shown);
    const int16_t numW   = card_.textWidth(numStr, 4);
    const int16_t pctW   = card_.textWidth("%", 2);
    const int16_t totalW = numW + 2 + pctW;
    const int16_t startX = kCardWidth - 12 - totalW;
    card_.setTextDatum(TL_DATUM);
    card_.drawString(numStr, startX, 4, 4);
    card_.drawString("%", startX + numW + 2, 14, 2);
  } else {
    char text[12];
    snprintf(text, sizeof(text), "%.0f%s", metric.shown, unit);
    card_.setTextDatum(TR_DATUM);
    card_.drawString(text, kCardWidth - 12, 4, 4);
    card_.setTextDatum(TL_DATUM);
  }

  liquid::bar(card_, 48, 34, kCardWidth - 60, 12, ratio, now, shade(calm), tone,
              background);

  card_.pushSprite(kMargin, y);
}

void BotDisplay::drawMetricCards(uint32_t now, bool volumeOverlay) {
  drawMetricCard(0, kCardFirstY, "CPU TEMPERATURE", cpu_, "\xb0\x43",
                 theme::kPrimary, true, now);
  // The overlay occupies this slot rather than being drawn on top of it.
  // Pushing both to the same region in one pass is what made the screen flicker
  // while the knob was moving.
  if (volumeOverlay) {
    drawVolumeOverlay(now);
  } else {
    drawMetricCard(1, kCardFirstY + kCardSpacing, "GPU TEMPERATURE", gpu_,
                   "\xb0\x43", theme::kSecondary, true, now);
  }
  drawMetricCard(2, kCardFirstY + kCardSpacing * 2, "MEMORY IN USE", memory_,
                 "%", theme::kTertiary, false, now, true);
}

void BotDisplay::drawProvisioning() {
  const uint16_t background = shade(theme::kInk);
  const uint16_t accent = shade(theme::kPrimary);
  display_.fillRect(0, kTileY, kScreenWidth, kFooterY - kTileY, background);
  theme::glassCard(display_, kMargin, kTileY, kScreenWidth - kMargin * 2,
                   kFooterY - kTileY - 8, 16, background, accent, 0.3f);

  display_.setTextColor(shade(theme::kMuted));
  display_.drawString("WI-FI HOTSPOT", kMargin + 16, kTileY + 20, 1);
  display_.setTextColor(shade(theme::kText));
  display_.drawString(setupSsid_, kMargin + 16, kTileY + 34, 4);

  display_.setTextColor(shade(theme::kMuted));
  display_.drawString("PASSWORD", kMargin + 16, kTileY + 76, 1);
  display_.setTextColor(shade(theme::kSuccess));
  display_.drawString(setupPassword_, kMargin + 16, kTileY + 90, 4);

  display_.setTextColor(shade(theme::kMuted));
  display_.drawString("THEN OPEN", kMargin + 16, kTileY + 132, 1);
  display_.setTextColor(shade(theme::kTertiary));
  display_.drawString(setupUrl_, kMargin + 16, kTileY + 146, 2);

  display_.setTextColor(shade(theme::kTextSoft));
  display_.drawString(moodHint(), kMargin + 16, kTileY + 184, 2);
  provisioningDirty_ = false;
}

void BotDisplay::drawFooter(uint32_t now) {
  // Rendered into a sprite and pushed in one write. Painting the gradient and
  // then the text straight to the panel made the speaker icon and the hold hint
  // flicker on every repaint.
  const uint16_t accent = shade(moodAccent());
  const int16_t height = kScreenHeight - kFooterY;
  theme::verticalGradient(footer_, 0, 0, kScreenWidth, height, shade(theme::kInk),
                          shade(theme::kBackdrop));

  if (touchPressed_) {
    liquid::bar(footer_, kMargin, 4, kScreenWidth - kMargin * 2, 14,
                holdProgress_, now, shade(theme::kPrimary),
                shade(theme::kTertiary), shade(theme::kInk));
    footer_.pushSprite(0, kFooterY);
    return;
  }

  if (volumeKnown_) {
    // The knob's current position, so the level is readable without moving it.
    const uint8_t waves = volumeLevel_ >= 66 ? 3 : (volumeLevel_ >= 33 ? 2 : 1);
    icons::speaker(footer_, kMargin, 3, 12, shade(theme::kTextSoft),
                   shade(theme::kBevelDark), waves,
                   volumeMuted_ || volumeLevel_ == 0);
    char text[10];
    snprintf(text, sizeof(text), "%u%%", volumeLevel_);
    footer_.setTextColor(shade(theme::kTextSoft));
    footer_.drawString(text, kMargin + 24, 6, 1);
  } else {
    footer_.setTextColor(shade(theme::kMuted));
    footer_.drawString(moodLabel(), kMargin, 6, 1);
  }
  footer_.setTextDatum(TR_DATUM);
  footer_.setTextColor(theme::mix(shade(theme::kMuted), accent, 0.5f));
  footer_.drawString("HOLD 2s - PC POWER", kScreenWidth - kMargin, 6, 1);
  footer_.setTextDatum(TL_DATUM);
  footer_.pushSprite(0, kFooterY);
}

void BotDisplay::finishIntro() {
  welcome_.skip();
  welcomeCanvas_.deleteSprite();

  // Dashboard sprites are allocated only after the intro canvas is released.
  net_.setColorDepth(16);
  card_.setColorDepth(16);
  footer_.setColorDepth(16);
  const bool ready =
      net_.createSprite(kTileWidth, kTileHeight) != nullptr &&
      card_.createSprite(kCardWidth, kCardHeight) != nullptr &&
      footer_.createSprite(kScreenWidth, kScreenHeight - kFooterY) != nullptr;
  spritesReady_ = ready;
  net_.setTextDatum(TL_DATUM);
  card_.setTextDatum(TL_DATUM);
  footer_.setTextDatum(TL_DATUM);

  display_.fillScreen(theme::kInk);
  if (!ready) {
    display_.setTextColor(theme::kDanger, theme::kInk);
    display_.drawCentreString("DISPLAY BUFFER ERROR", 120, 140, 4);
    return;
  }
  chromeDirty_ = true;
  provisioningDirty_ = true;
}

void BotDisplay::updateSleep(uint32_t now) {
  const bool keepAwake = mood_ == BotMood::Warning ||
                         mood_ == BotMood::Critical ||
                         mood_ == BotMood::Provisioning ||
                         mood_ == BotMood::Disconnected || touchPressed_;
  if (!keepAwake && mood_ != BotMood::Sleeping &&
      now - lastActivity_ > BoardConfig::kIdleSleepMs) {
    preSleepMood_ = baseMood_;
    mood_ = BotMood::Sleeping;
    message_ = "";
    moodStarted_ = now;
    chromeDirty_ = true;
  }

  const float target = mood_ == BotMood::Sleeping ? kSleepDimTarget : 0.0f;
  const float previous = sleepDim_;
  sleepDim_ = approach(sleepDim_, target, 0.07f);
  theme::setDim(sleepDim_);
  if (fabsf(previous - sleepDim_) > 0.001f) {
    chromeDirty_ = true;
    provisioningDirty_ = true;
  }
}

void BotDisplay::updateTransientMood(uint32_t now) {
  if (mood_ != BotMood::Petted)
    return;
  if (now - moodStarted_ < BoardConfig::kTransientMoodMs)
    return;
  mood_ = baseMood_;
  message_ = "";
  moodStarted_ = now;
  chromeDirty_ = true;
}

bool BotDisplay::wantsClock(uint32_t now) const {
  if (!pcStateKnown_ || pcOnline_ || touchPressed_)
    return false;
  if (mood_ == BotMood::Provisioning || mood_ == BotMood::Resetting)
    return false;
  return now - lastActivity_ > BoardConfig::kClockDelayMs;
}

void BotDisplay::enterClock() {
  clockMode_ = true;
  sleepDim_ = BoardConfig::kPowerSaverDim;
  theme::setDim(sleepDim_);
  clock_.reset();
  display_.fillScreen(theme::tone(theme::kInk));
}

void BotDisplay::leaveClock() {
  clockMode_ = false;
  sleepDim_ = 0.0f;
  theme::setDim(0.0f);
  display_.fillScreen(theme::kInk);
  chromeDirty_ = true;
  provisioningDirty_ = true;
}

void BotDisplay::tick(uint32_t now) {
  // The update screen is painted by setUpdateProgress and owns the panel until
  // the device reboots into the new image.
  if (updating_) return;
  const uint32_t interval = clockMode_ ? BoardConfig::kPowerSaverFrameMs
                                       : BoardConfig::kDisplayFrameMs;
  if (now - lastFrame_ < interval)
    return;
  lastFrame_ = now;
  frame_++;
  pressGlow_ = approach(pressGlow_, touchPressed_ ? 1.0f : 0.0f, 0.12f);

  if (welcome_.active()) {
    if (!welcome_.tick(welcomeCanvas_, kWelcomeX, kWelcomeY, now))
      finishIntro();
    return;
  }
  if (!spritesReady_)
    return;

  const bool clockWanted = wantsClock(now);
  if (clockWanted != clockMode_) {
    if (clockWanted) {
      enterClock();
    } else {
      leaveClock();
    }
  }
  if (clockMode_) {
    // 1.7e9 is 2023-11-14: anything earlier means NTP has not answered yet.
    const bool timeValid = time(nullptr) > 1700000000;
    clock_.render(display_, now, theme::tone(theme::kPrimary), timeValid);
    return;
  }

  updateTransientMood(now);
  updateSleep(now);
  updateValues();

  if (chromeDirty_) {
    drawHeader(now);
    chromeDirty_ = false;
  }

  const bool volumeOverlay = now < volumeOverlayUntil_;
  if (mood_ == BotMood::Provisioning && !setupSsid_.isEmpty()) {
    if (provisioningDirty_)
      drawProvisioning();
  } else if (now - lastCardFrame_ >= BoardConfig::kCardFrameMs) {
    lastCardFrame_ = now;
    // The now-playing card is for music that is actually playing. Paused or
    // stopped hands the slot back to the network graph.
    if (mediaPlaying_) {
      drawMedia(now);
    } else {
      drawNetwork(now);
    }
    drawMetricCards(now, volumeOverlay);
  }

  if (touchPressed_ || frame_ % 4 == 0)
    drawFooter(now);
}
