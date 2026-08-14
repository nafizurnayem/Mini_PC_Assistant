#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

// Shared colour palette and the small set of "2.5D" drawing primitives used by
// every screen: gradients, bevelled glass panels, neon glows, shaded spheres
// and bars. Everything takes a TFT_eSPI reference so the same code draws
// straight to the panel or into a sprite.
namespace theme {

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// Deep navy-violet ground with frosted card surfaces. The accent is a single
// violet-to-blue gradient; status colours are deliberately separate from it so
// "needs attention" never reads as decoration.
constexpr uint16_t kInk = rgb(8, 10, 24);
constexpr uint16_t kBackdrop = rgb(14, 18, 40);
constexpr uint16_t kSurfaceTop = rgb(30, 38, 70);
constexpr uint16_t kSurfaceBottom = rgb(15, 20, 44);
constexpr uint16_t kWellTop = rgb(9, 12, 28);
constexpr uint16_t kWellBottom = rgb(18, 24, 50);
constexpr uint16_t kBevelLight = rgb(126, 146, 224);
constexpr uint16_t kBevelDark = rgb(4, 5, 14);
constexpr uint16_t kText = rgb(238, 242, 255);
constexpr uint16_t kTextSoft = rgb(166, 180, 216);
constexpr uint16_t kMuted = rgb(108, 122, 158);
constexpr uint16_t kWhite = rgb(255, 255, 255);

constexpr uint16_t kPrimary = rgb(139, 92, 246);    // violet, gradient start
constexpr uint16_t kSecondary = rgb(59, 130, 246);  // blue, gradient end
constexpr uint16_t kTertiary = rgb(34, 211, 238);   // cyan highlight
constexpr uint16_t kSuccess = rgb(34, 197, 94);
constexpr uint16_t kWarning = rgb(245, 158, 11);
constexpr uint16_t kDanger = rgb(244, 63, 94);

// Blend two 565 colours. amount 0 returns a, 1 returns b.
uint16_t mix(uint16_t a, uint16_t b, float amount);
uint16_t lighten(uint16_t color, float amount);
uint16_t darken(uint16_t color, float amount);

// Global dim used by standby and power saver. Every primitive routes its
// colours through tone(), so one value fades the whole interface consistently.
void setDim(float amount);
uint16_t tone(uint16_t color);

// Easing helpers used by the welcome animation and UI transitions.
float clamp01(float value);
float easeOutCubic(float t);
float easeOutBack(float t);
float smoothStep(float t);

void verticalGradient(TFT_eSPI& g, int16_t x, int16_t y, int16_t w, int16_t h,
                      uint16_t top, uint16_t bottom);
void gradientRoundRect(TFT_eSPI& g, int16_t x, int16_t y, int16_t w, int16_t h,
                       int16_t radius, uint16_t top, uint16_t bottom);

// Neon bloom: concentric outlines fading into the background. Draw before the
// panel it belongs to.
void glow(TFT_eSPI& g, int16_t x, int16_t y, int16_t w, int16_t h,
          int16_t radius, uint16_t color, uint16_t background,
          uint8_t rings = 3);

// Glass panel: drop shadow, gradient body, light top bevel, dark bottom bevel,
// a sheen line and a tinted border. Set raised=false for an inset "well".
void panel(TFT_eSPI& g, int16_t x, int16_t y, int16_t w, int16_t h,
           int16_t radius, uint16_t border, bool raised = true);

// Frosted card: translucent-looking body over the ground, a lit top-left rim,
// a dark bottom-right rim, and a diagonal sheen. This is the base surface for
// every dashboard tile.
void glassCard(TFT_eSPI& g, int16_t x, int16_t y, int16_t w, int16_t h,
               int16_t radius, uint16_t background, uint16_t tint = 0,
               float tintStrength = 0.0f);

void horizontalGradient(TFT_eSPI& g, int16_t x, int16_t y, int16_t w, int16_t h,
                        uint16_t left, uint16_t right);

// Filled rounded rect with a horizontal gradient, used for gradient buttons and
// pills.
void gradientPill(TFT_eSPI& g, int16_t x, int16_t y, int16_t w, int16_t h,
                  uint16_t left, uint16_t right);

// Moving highlight band across an already-drawn sprite. `position` is 0..1
// across the width; values outside that range draw nothing.
void sheenSweep(TFT_eSPI& g, int16_t x, int16_t y, int16_t w, int16_t h,
                float position, float strength);

// Per-pixel shaded sphere with specular highlight and fresnel rim light.
void sphere(TFT_eSPI& g, int16_t cx, int16_t cy, int16_t r, uint16_t base,
            uint16_t rim, uint16_t background, float gloss = 1.0f);

// Recessed track with a glowing filled portion. progress is 0..1.
void glassBar(TFT_eSPI& g, int16_t x, int16_t y, int16_t w, int16_t h,
              float progress, uint16_t accent);

// Text with a one pixel drop shadow, which is what makes flat fonts read as
// embossed against a dark surface.
void embossedText(TFT_eSPI& g, const String& text, int16_t x, int16_t y,
                  uint8_t font, uint16_t color, uint16_t shadow);

// Text with a coloured halo behind it, for neon labels.
void neonText(TFT_eSPI& g, const String& text, int16_t x, int16_t y,
              uint8_t font, uint16_t color, uint16_t background);

}  // namespace theme
