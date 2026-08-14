#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

// Power-saver clock shown while the PC is off. Static chrome is painted once
// and the time only when the displayed string changes, so the panel costs
// almost nothing to keep alive.
class ClockScreen {
 public:
  void reset();
  void render(TFT_eSPI& display, uint32_t now, uint16_t accent, bool timeValid);

 private:
  void drawChrome(TFT_eSPI& display, uint16_t accent);
  void drawTime(TFT_eSPI& display, uint16_t accent, const String& timeText,
                const String& meridiem, const String& dateText);

  String lastTime_;
  String lastDate_;
  String lastMeridiem_;
  bool chromeDrawn_ = false;
};
