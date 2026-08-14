#include "DisplayTheme.h"

#include <math.h>

namespace theme {
namespace {
constexpr float kLightX = -0.42f;
constexpr float kLightY = -0.56f;
constexpr float kLightZ = 0.72f;
constexpr float kEdgeSoftness = 0.06f;

float dimAmount = 0.0f;

struct Channels {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

Channels split(uint16_t color) {
  return {static_cast<uint8_t>((color >> 11) & 0x1F),
          static_cast<uint8_t>((color >> 5) & 0x3F),
          static_cast<uint8_t>(color & 0x1F)};
}

uint16_t pack(int r, int g, int b) {
  if (r < 0) r = 0;
  if (g < 0) g = 0;
  if (b < 0) b = 0;
  if (r > 31) r = 31;
  if (g > 63) g = 63;
  if (b > 31) b = 31;
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

// diff^24 without powf: three squarings plus one multiply.
float specular(float diffuse) {
  const float d2 = diffuse * diffuse;
  const float d4 = d2 * d2;
  const float d8 = d4 * d4;
  return d8 * d8 * d8;
}
}  // namespace

uint16_t mix(uint16_t a, uint16_t b, float amount) {
  amount = clamp01(amount);
  const Channels ca = split(a);
  const Channels cb = split(b);
  return pack(static_cast<int>(ca.r + (cb.r - ca.r) * amount + 0.5f),
              static_cast<int>(ca.g + (cb.g - ca.g) * amount + 0.5f),
              static_cast<int>(ca.b + (cb.b - ca.b) * amount + 0.5f));
}

uint16_t lighten(uint16_t color, float amount) { return mix(color, kWhite, amount); }

uint16_t darken(uint16_t color, float amount) { return mix(color, 0x0000, amount); }

void setDim(float amount) { dimAmount = clamp01(amount); }

uint16_t tone(uint16_t color) {
  if (dimAmount <= 0.01f) return color;
  return mix(color, kInk, dimAmount);
}

float clamp01(float value) {
  if (value < 0.0f) return 0.0f;
  if (value > 1.0f) return 1.0f;
  return value;
}

float easeOutCubic(float t) {
  const float inverted = 1.0f - clamp01(t);
  return 1.0f - inverted * inverted * inverted;
}

float easeOutBack(float t) {
  constexpr float kOvershoot = 1.70158f;
  const float inverted = clamp01(t) - 1.0f;
  return 1.0f + (kOvershoot + 1.0f) * inverted * inverted * inverted +
         kOvershoot * inverted * inverted;
}

float smoothStep(float t) {
  t = clamp01(t);
  return t * t * (3.0f - 2.0f * t);
}

void verticalGradient(TFT_eSPI& g, int16_t x, int16_t y, int16_t w, int16_t h,
                      uint16_t top, uint16_t bottom) {
  if (h <= 0 || w <= 0) return;
  for (int16_t row = 0; row < h; ++row) {
    const float t = h > 1 ? static_cast<float>(row) / (h - 1) : 0.0f;
    g.drawFastHLine(x, y + row, w, mix(top, bottom, t));
  }
}

void gradientRoundRect(TFT_eSPI& g, int16_t x, int16_t y, int16_t w, int16_t h,
                       int16_t radius, uint16_t top, uint16_t bottom) {
  if (h <= 0 || w <= 0) return;
  if (radius > w / 2) radius = w / 2;
  if (radius > h / 2) radius = h / 2;
  const float radiusF = static_cast<float>(radius);
  for (int16_t row = 0; row < h; ++row) {
    int16_t inset = 0;
    const int16_t fromTop = row;
    const int16_t fromBottom = h - 1 - row;
    const int16_t nearest = fromTop < fromBottom ? fromTop : fromBottom;
    if (nearest < radius) {
      const float dy = radiusF - nearest - 0.5f;
      const float chord = sqrtf(radiusF * radiusF - dy * dy);
      inset = static_cast<int16_t>(radiusF - chord + 0.5f);
    }
    const float t = h > 1 ? static_cast<float>(row) / (h - 1) : 0.0f;
    g.drawFastHLine(x + inset, y + row, w - inset * 2, mix(top, bottom, t));
  }
}

void glow(TFT_eSPI& g, int16_t x, int16_t y, int16_t w, int16_t h,
          int16_t radius, uint16_t color, uint16_t background, uint8_t rings) {
  for (uint8_t ring = rings; ring >= 1; --ring) {
    const float falloff = 0.42f / (ring * ring);
    g.drawRoundRect(x - ring, y - ring, w + ring * 2, h + ring * 2,
                    radius + ring, mix(background, color, falloff));
  }
}

void panel(TFT_eSPI& g, int16_t x, int16_t y, int16_t w, int16_t h,
           int16_t radius, uint16_t border, bool raised) {
  const uint16_t surfaceTop = tone(kSurfaceTop);
  const uint16_t surfaceBottom = tone(kSurfaceBottom);
  const uint16_t bevelDark = tone(kBevelDark);
  if (raised) {
    // Soft drop shadow: two offset outlines instead of a real blur.
    g.drawRoundRect(x + 1, y + 2, w, h, radius, darken(tone(kInk), 0.25f));
    g.drawRoundRect(x, y + 1, w, h, radius, bevelDark);
    gradientRoundRect(g, x, y, w, h, radius, surfaceTop, surfaceBottom);
    // Sheen: a brighter band just under the top edge, like light across glass.
    if (h > 12) {
      g.drawFastHLine(x + radius, y + 2, w - radius * 2,
                      lighten(surfaceTop, 0.16f));
    }
    g.drawFastHLine(x + radius, y, w - radius * 2, lighten(surfaceTop, 0.30f));
    g.drawFastHLine(x + radius, y + h - 1, w - radius * 2, bevelDark);
    g.drawFastVLine(x, y + radius, h - radius * 2, lighten(surfaceTop, 0.12f));
    g.drawFastVLine(x + w - 1, y + radius, h - radius * 2, bevelDark);
  } else {
    const uint16_t wellBottom = tone(kWellBottom);
    gradientRoundRect(g, x, y, w, h, radius, tone(kWellTop), wellBottom);
    g.drawFastHLine(x + radius, y, w - radius * 2, bevelDark);
    g.drawFastHLine(x + radius, y + h - 1, w - radius * 2,
                    lighten(wellBottom, 0.22f));
  }
  g.drawRoundRect(x, y, w, h, radius, mix(surfaceTop, border, 0.45f));
}

void horizontalGradient(TFT_eSPI& g, int16_t x, int16_t y, int16_t w, int16_t h,
                        uint16_t left, uint16_t right) {
  if (w <= 0 || h <= 0) return;
  for (int16_t column = 0; column < w; ++column) {
    const float t = w > 1 ? static_cast<float>(column) / (w - 1) : 0.0f;
    g.drawFastVLine(x + column, y, h, mix(left, right, t));
  }
}

void gradientPill(TFT_eSPI& g, int16_t x, int16_t y, int16_t w, int16_t h,
                  uint16_t left, uint16_t right) {
  if (w <= 0 || h <= 0) return;
  const int16_t radius = h / 2;
  const float radiusF = static_cast<float>(radius);
  for (int16_t row = 0; row < h; ++row) {
    const int16_t fromTop = row;
    const int16_t fromBottom = h - 1 - row;
    const int16_t nearest = fromTop < fromBottom ? fromTop : fromBottom;
    int16_t inset = 0;
    if (nearest < radius) {
      const float dy = radiusF - nearest - 0.5f;
      inset = static_cast<int16_t>(radiusF - sqrtf(radiusF * radiusF - dy * dy) + 0.5f);
    }
    for (int16_t column = inset; column < w - inset; ++column) {
      const float t = w > 1 ? static_cast<float>(column) / (w - 1) : 0.0f;
      uint16_t color = mix(left, right, t);
      // Top third catches more light, like a glossy capsule.
      if (row < h / 3) color = lighten(color, 0.18f * (1.0f - row / (h / 3.0f)));
      g.drawPixel(x + column, y + row, color);
    }
  }
}

void glassCard(TFT_eSPI& g, int16_t x, int16_t y, int16_t w, int16_t h,
               int16_t radius, uint16_t background, uint16_t tint,
               float tintStrength) {
  // Body: a lifted, slightly blue-shifted version of the ground, which is how
  // frosted glass reads without real transparency.
  uint16_t top = mix(background, kSurfaceTop, 0.92f);
  uint16_t bottom = mix(background, kSurfaceBottom, 0.92f);
  if (tintStrength > 0.0f) {
    top = mix(top, tint, tintStrength * 0.35f);
    bottom = mix(bottom, tint, tintStrength * 0.15f);
  }
  g.drawRoundRect(x + 1, y + 2, w, h, radius, mix(background, kBevelDark, 0.65f));
  gradientRoundRect(g, x, y, w, h, radius, top, bottom);

  // Lit top-left rim, dark bottom-right rim.
  const uint16_t rimLight = mix(top, kBevelLight, 0.55f);
  const uint16_t rimDark = mix(bottom, kBevelDark, 0.65f);
  g.drawFastHLine(x + radius, y, w - radius * 2, rimLight);
  g.drawFastVLine(x, y + radius, h - radius * 2 - radius / 2, rimLight);
  g.drawFastHLine(x + radius, y + h - 1, w - radius * 2, rimDark);
  g.drawFastVLine(x + w - 1, y + radius + radius / 2, h - radius * 2 - radius / 2,
                  rimDark);
  g.drawRoundRect(x, y, w, h, radius, mix(top, kBevelLight, 0.22f));

  // Diagonal sheen across the upper-left corner.
  const int16_t reach = w / 3;
  for (int16_t i = 0; i < reach; ++i) {
    const float fade = 1.0f - static_cast<float>(i) / reach;
    g.drawPixel(x + radius + i, y + 2 + i / 3, mix(top, kWhite, 0.16f * fade));
    g.drawPixel(x + radius + i, y + 3 + i / 3, mix(top, kWhite, 0.07f * fade));
  }
}

void sheenSweep(TFT_eSPI& g, int16_t x, int16_t y, int16_t w, int16_t h,
                float position, float strength) {
  if (position < 0.0f || position > 1.0f) return;
  const int16_t band = w / 6 > 6 ? w / 6 : 6;
  const int16_t head = x + static_cast<int16_t>(position * (w + band)) - band;
  for (int16_t column = head; column < head + band; ++column) {
    if (column < x || column >= x + w) continue;
    const float edge =
        1.0f - fabsf((head + band * 0.5f) - column) / (band * 0.5f);
    if (edge <= 0.0f) continue;
    for (int16_t row = y; row < y + h; ++row) {
      const uint16_t current = g.readPixel(column, row);
      g.drawPixel(column, row, lighten(current, edge * strength));
    }
  }
}


void sphere(TFT_eSPI& g, int16_t cx, int16_t cy, int16_t r, uint16_t base,
            uint16_t rim, uint16_t background, float gloss) {
  if (r <= 0) return;
  const uint16_t shadowTone = darken(base, 0.78f);
  const float radiusF = static_cast<float>(r);
  for (int16_t dy = -r; dy <= r; ++dy) {
    const float ny = dy / radiusF;
    const float nySquared = ny * ny;
    if (nySquared > 1.0f) continue;
    const int16_t span = static_cast<int16_t>(sqrtf(1.0f - nySquared) * radiusF);
    for (int16_t dx = -span; dx <= span; ++dx) {
      const float nx = dx / radiusF;
      const float lengthSquared = nx * nx + nySquared;
      if (lengthSquared > 1.0f) continue;
      const float nz = sqrtf(1.0f - lengthSquared);

      float diffuse = nx * kLightX + ny * kLightY + nz * kLightZ;
      if (diffuse < 0.0f) diffuse = 0.0f;
      uint16_t color = mix(shadowTone, base, 0.16f + 0.84f * diffuse * diffuse);
      color = mix(color, kWhite, specular(diffuse) * 0.9f * gloss);

      const float fresnel = (1.0f - nz) * (1.0f - nz) * (1.0f - nz);
      color = mix(color, rim, fresnel * 0.6f);

      // Fade the last few percent of the radius into the background so the
      // silhouette does not look stair-stepped.
      if (lengthSquared > 1.0f - kEdgeSoftness) {
        color = mix(background, color, (1.0f - lengthSquared) / kEdgeSoftness);
      }
      g.drawPixel(cx + dx, cy + dy, color);
    }
  }
}

void glassBar(TFT_eSPI& g, int16_t x, int16_t y, int16_t w, int16_t h,
              float progress, uint16_t accent) {
  const int16_t radius = h / 2;
  gradientRoundRect(g, x, y, w, h, radius, tone(kWellTop), tone(kWellBottom));
  g.drawRoundRect(x, y, w, h, radius, tone(kBevelDark));

  const int16_t filled = static_cast<int16_t>(clamp01(progress) * w);
  if (filled < 2) return;
  gradientRoundRect(g, x, y, filled, h, radius, lighten(accent, 0.55f),
                    darken(accent, 0.22f));
  if (h >= 5) {
    // Bright core line: the neon "tube" inside the glass.
    g.drawFastHLine(x + radius, y + 1, filled - radius * 2,
                    lighten(accent, 0.80f));
    g.drawFastHLine(x + radius, y + h - 2, filled - radius * 2,
                    darken(accent, 0.45f));
  }
  if (filled > radius * 2) {
    g.drawFastVLine(x + filled - 1, y + 1, h - 2, lighten(accent, 0.9f));
  }
}

void neonText(TFT_eSPI& g, const String& text, int16_t x, int16_t y,
              uint8_t font, uint16_t color, uint16_t background) {
  const uint16_t halo = mix(background, color, 0.30f);
  g.setTextColor(halo);
  g.drawString(text, x - 1, y, font);
  g.drawString(text, x + 1, y, font);
  g.drawString(text, x, y + 1, font);
  g.setTextColor(color);
  g.drawString(text, x, y, font);
}


void embossedText(TFT_eSPI& g, const String& text, int16_t x, int16_t y,
                  uint8_t font, uint16_t color, uint16_t shadow) {
  g.setTextColor(shadow);
  g.drawString(text, x + 1, y + 1, font);
  g.setTextColor(color);
  g.drawString(text, x, y, font);
}

}  // namespace theme
