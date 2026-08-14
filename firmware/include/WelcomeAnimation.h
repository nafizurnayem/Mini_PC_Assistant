#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

// Boot splash that replaces the old red/green/blue panel flash. It still proves
// the display is alive (backdrop, motion, text and colour all appear within the
// first second) but reads as a product intro instead of a test pattern.
class WelcomeAnimation {
 public:
  void begin(TFT_eSPI& display, uint32_t now);
  // Renders one frame. Returns false once the intro has finished.
  bool tick(TFT_eSprite& canvas, int16_t originX, int16_t originY, uint32_t now);
  bool active() const { return active_; }
  void skip() { active_ = false; }

 private:
  void drawBackdrop(TFT_eSPI& display);
  void drawOrb(TFT_eSprite& canvas, float progress);
  void drawWordmark(TFT_eSprite& canvas, float progress);
  void drawShine(TFT_eSprite& canvas, float progress);
  void drawFooter(TFT_eSprite& canvas, float progress);

  uint32_t started_ = 0;
  bool active_ = false;
};
