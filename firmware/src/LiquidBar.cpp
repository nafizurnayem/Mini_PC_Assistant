#include "LiquidBar.h"

#include <math.h>

#include "DisplayTheme.h"

namespace liquid {
namespace {
constexpr float kTwoPi = 6.2831853f;
constexpr uint8_t kMaxBubbles = 12;

float fract(float value) { return value - floorf(value); }

// Deterministic per-bubble jitter, so the field looks random but never needs
// state carried between frames.
float hash(uint8_t index, float salt) {
  return fract(sinf(index * 12.9898f + salt) * 43758.5453f);
}
}  // namespace

void bar(TFT_eSPI& g, int16_t x, int16_t y, int16_t w, int16_t h, float progress,
         uint32_t now, uint16_t from, uint16_t to, uint16_t background) {
  if (w < 6 || h < 4) return;
  const int16_t radius = h / 2;

  // Recessed track with a dark inner rim.
  theme::gradientRoundRect(g, x, y, w, h, radius, theme::tone(theme::kWellTop),
                           theme::tone(theme::kWellBottom));
  g.drawRoundRect(x, y, w, h, radius, theme::mix(background, theme::kBevelDark, 0.7f));

  progress = theme::clamp01(progress);
  const int16_t usable = w - 2;
  const int16_t fill = static_cast<int16_t>(progress * usable);
  if (fill < 2) return;

  // Body: horizontal gradient with a flat front edge, clipped by the rounded
  // track.
  for (int16_t row = 1; row < h - 1; ++row) {
    const int16_t fromTop = row;
    const int16_t fromBottom = h - 1 - row;
    const int16_t nearest = fromTop < fromBottom ? fromTop : fromBottom;
    int16_t inset = 1;
    if (nearest < radius) {
      const float dy = radius - nearest - 0.5f;
      inset = static_cast<int16_t>(radius - sqrtf(radius * radius - dy * dy) + 0.5f);
      if (inset < 1) inset = 1;
    }
    for (int16_t column = inset; column < fill && column < w - inset; ++column) {
      const float t = static_cast<float>(column) / (usable > 1 ? usable - 1 : 1);
      uint16_t color = theme::mix(from, to, theme::clamp01(t));
      // Depth: lighter at the top of the tube, darker towards the bottom.
      const float depth = static_cast<float>(row) / (h - 1);
      color = theme::mix(theme::lighten(color, 0.22f), theme::darken(color, 0.30f),
                         depth);
      g.drawPixel(x + column, y + row, color);
    }
  }

  const uint16_t frontColor = theme::mix(from, to, theme::clamp01(progress));
  // Bright leading edge.
  if (fill > 2 && fill < w - 1) {
    g.drawFastVLine(x + fill - 1, y + 2, h - 4, theme::lighten(frontColor, 0.55f));
  }

  // Rising bubbles carried towards the leading edge. Each one is derived from
  // its index and the clock, so no per-bar state is stored.
  const uint8_t count = static_cast<uint8_t>(fill / 14 + 3) < kMaxBubbles
                            ? static_cast<uint8_t>(fill / 14 + 3)
                            : kMaxBubbles;
  const float seconds = now / 1000.0f;
  for (uint8_t index = 0; index < count; ++index) {
    const float speed = 0.06f + hash(index, 1.7f) * 0.10f;
    const float lane = hash(index, 4.2f);
    const float drift = fract(hash(index, 9.1f) + seconds * speed);

    const int16_t bx = x + 2 + static_cast<int16_t>(drift * (fill - 4));
    if (bx <= x + 1 || bx >= x + fill - 2) continue;

    // Bubbles ride upwards as they travel, wobbling slightly on the way.
    const float rise = 0.72f - drift * 0.42f;
    const float wobble = sinf(seconds * (1.4f + lane) + index) * (h * 0.07f);
    const int16_t by = y + 2 + static_cast<int16_t>((h - 4) * rise * lane + wobble);
    if (by <= y + 1 || by >= y + h - 2) continue;

    // Fade in on entry and out as they reach the surface.
    const float life = drift < 0.12f ? drift / 0.12f
                                     : (drift > 0.88f ? (1.0f - drift) / 0.12f : 1.0f);
    const uint16_t base = theme::mix(from, to, drift);
    const uint16_t bubble = theme::lighten(base, 0.45f + 0.35f * life);

    if (h >= 12 && index % 3 == 0) {
      // Larger bubbles get a hollow rim and a highlight dot.
      g.drawCircle(bx, by, 2, bubble);
      g.drawPixel(bx - 1, by - 1, theme::lighten(base, 0.85f));
    } else {
      g.drawPixel(bx, by, bubble);
      if (h >= 10) g.drawPixel(bx, by - 1, theme::mix(base, bubble, 0.5f));
    }
  }
}


}  // namespace liquid
