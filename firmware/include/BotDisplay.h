#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "ClockScreen.h"
#include "WelcomeAnimation.h"

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

// Portrait 240x320 telemetry dashboard built from frosted glass cards: a
// summary row (load ring plus history chart) over three metric cards with
// animated liquid fill bars.
class BotDisplay {
 public:
  // Throughput history length. At the 5 s telemetry interval this is about
  // four minutes of trend.
  static constexpr uint8_t kHistorySamples = 46;

  BotDisplay();
  void begin();
  void setMood(BotMood mood, const String& message = "");
  void setMetrics(float cpuTemp, float gpuTemp, float memoryPercent);
  // Network throughput from PC telemetry plus the controller's own link
  // strength in dBm (0 means "not associated").
  void setNetwork(float downloadMbps, float uploadMbps, int16_t rssiDbm);
  // Now playing on the PC. `status` is "playing", "paused" or "stopped";
  // position and duration may be NAN when the player does not report them.
  void setMedia(const String& status, const String& title, const String& artist,
                const String& app, float positionSeconds, float durationSeconds);
  // PC master volume. Shown as an overlay while it is moving.
  void setVolume(uint8_t level, bool muted);
  void setProvisioningInfo(const String& ssid, const String& password,
                           const String& url);
  // Live touch state so the interface reacts on the same frame the finger
  // lands. holdProgress is 0..1 towards the power-command hold threshold.
  void setTouch(bool pressed, float holdProgress);
  // Reported PC power state. Once the PC is known to be off, the display drops
  // into the power-saver clock instead of animating a dashboard with no data.
  void setPcOnline(bool online);
  void tick(uint32_t now);
  void noteActivity(uint32_t now);
  bool introRunning() const { return welcome_.active(); }
  bool powerSaverActive() const { return clockMode_; }
  BotMood mood() const { return mood_; }

 private:
  struct Metric {
    float target = NAN;
    float shown = NAN;
  };

  void drawHeader(uint32_t now);
  void drawNetwork(uint32_t now);
  void drawMedia(uint32_t now);
  void drawVolumeOverlay(uint32_t now);
  float mediaPositionAt(uint32_t now) const;
  void drawTrace(const uint8_t* samples, int16_t plotX, int16_t plotY,
                 int16_t plotW, int16_t plotH, uint16_t color, bool filled,
                 uint32_t now);
  void drawMetricCard(uint8_t index, int16_t y, const char* label,
                      const Metric& metric, const char* unit, uint16_t calm,
                      bool temperature, uint32_t now, bool unitPrefix = false);
  void drawMetricCards(uint32_t now);
  void drawProvisioning();
  void drawFooter(uint32_t now);
  void finishIntro();
  void updateSleep(uint32_t now);
  void updateTransientMood(uint32_t now);
  void updateValues();
  bool wantsClock(uint32_t now) const;
  void enterClock();
  void leaveClock();

  uint16_t moodAccent() const;
  const char* moodLabel() const;
  const char* moodHint() const;
  uint16_t shade(uint16_t color) const;
  uint8_t signalBars() const;

  TFT_eSPI display_;
  TFT_eSprite welcomeCanvas_;  // Freed once the intro finishes.
  TFT_eSprite net_;            // Full-width network activity graph.
  TFT_eSprite card_;           // Reused for each metric card.
  WelcomeAnimation welcome_;
  ClockScreen clock_;

  BotMood mood_ = BotMood::Boot;
  BotMood baseMood_ = BotMood::Boot;
  BotMood preSleepMood_ = BotMood::Happy;
  String message_;
  Metric cpu_;
  Metric gpu_;
  Metric memory_;
  Metric download_;
  Metric upload_;
  String setupSsid_;
  String setupPassword_;
  String setupUrl_;

  String mediaTitle_;
  String mediaArtist_;
  String mediaApp_;
  bool mediaActive_ = false;
  bool mediaPlaying_ = false;
  float mediaPosition_ = NAN;
  float mediaDuration_ = NAN;
  uint32_t mediaStamp_ = 0;

  uint8_t volumeLevel_ = 0;
  bool volumeMuted_ = false;
  bool volumeKnown_ = false;
  uint32_t volumeOverlayUntil_ = 0;
  bool volumeOverlayDrawn_ = false;

  int16_t rssiDbm_ = 0;
  // Decaying peak throughput, so the trend stays meaningful on any connection
  // speed instead of being pinned to a guessed maximum.
  float throughputPeak_ = 5.0f;

  // Throughput history in quarter-Mb steps (so 63.75 Mb fits a byte at useful
  // resolution); 255 means "no reading".
  uint8_t downHistory_[kHistorySamples];
  uint8_t upHistory_[kHistorySamples];
  uint8_t historyCount_ = 0;
  uint8_t historyHead_ = 0;

  bool pcOnline_ = true;
  bool pcStateKnown_ = false;
  bool clockMode_ = false;
  bool touchPressed_ = false;
  float holdProgress_ = 0.0f;
  float sleepDim_ = 0.0f;
  float pressGlow_ = 0.0f;

  uint32_t lastFrame_ = 0;
  uint32_t lastCardFrame_ = 0;
  uint32_t lastActivity_ = 0;
  uint32_t moodStarted_ = 0;
  uint32_t frame_ = 0;
  bool spritesReady_ = false;
  bool chromeDirty_ = true;
  bool provisioningDirty_ = true;
};
