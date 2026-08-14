#include "WelcomeAnimation.h"

#include <math.h>

#include "BoardConfig.h"
#include "DisplayTheme.h"
#include "LiquidBar.h"

namespace {
constexpr int16_t kScreenWidth = 240;
constexpr int16_t kScreenHeight = 320;
constexpr int16_t kMarkY = 44;
constexpr int16_t kMarkSize = 52;
constexpr int16_t kWordmarkY = 90;
constexpr int16_t kSuffixY = 118;
constexpr int16_t kBarY = 140;
constexpr int16_t kLetterGap = 4;

// Phase boundaries expressed as a fraction of BoardConfig::kWelcomeMs.
constexpr float kMarkStart = 0.02f;
constexpr float kMarkEnd = 0.34f;
constexpr float kNameStart = 0.26f;
constexpr float kNameEnd = 0.56f;
constexpr float kSuffixStart = 0.40f;
constexpr float kSuffixEnd = 0.70f;
constexpr float kShineStart = 0.58f;
constexpr float kShineEnd = 0.90f;

float phase(float progress, float from, float to) {
  if (progress <= from) return 0.0f;
  if (progress >= to) return 1.0f;
  return (progress - from) / (to - from);
}

uint16_t rowBackground(int16_t originY, int16_t row) {
  const float t = theme::clamp01(static_cast<float>(originY + row) / kScreenHeight);
  return theme::mix(theme::kBackdrop, theme::kInk, t);
}

int16_t spacedTextWidth(TFT_eSprite& canvas, const char* text, uint8_t font,
                        int16_t gap) {
  int16_t width = 0;
  for (const char* c = text; *c; ++c) {
    width += canvas.textWidth(String(*c), font) + gap;
  }
  return width - gap;
}

void drawSpacedText(TFT_eSprite& canvas, const char* text, int16_t centerX,
                    int16_t y, uint8_t font, int16_t gap, uint16_t color) {
  int16_t x = centerX - spacedTextWidth(canvas, text, font, gap) / 2;
  canvas.setTextColor(color);
  for (const char* c = text; *c; ++c) {
    const String glyph(*c);
    canvas.drawString(glyph, x, y, font);
    x += canvas.textWidth(glyph, font) + gap;
  }
}
}  // namespace

void WelcomeAnimation::begin(TFT_eSPI& display, uint32_t now) {
  started_ = now;
  active_ = true;
  drawBackdrop(display);
}

void WelcomeAnimation::drawBackdrop(TFT_eSPI& display) {
  theme::verticalGradient(display, 0, 0, kScreenWidth, kScreenHeight,
                          theme::kBackdrop, theme::kInk);
  for (int16_t i = 0; i < 22; ++i) {
    const uint16_t edge = theme::darken(theme::kInk, 0.55f - i * 0.024f);
    display.drawFastVLine(i, 0, kScreenHeight, edge);
    display.drawFastVLine(kScreenWidth - 1 - i, 0, kScreenHeight, edge);
  }
}

// The logo mark: a gradient rounded square with a glass highlight, the same
// language as the dashboard's icon tiles.
void WelcomeAnimation::drawOrb(TFT_eSprite& canvas, float progress) {
  const float grow = phase(progress, kMarkStart, kMarkEnd);
  if (grow <= 0.0f) return;
  const float scale = theme::easeOutBack(grow);
  const int16_t size = static_cast<int16_t>(kMarkSize * scale);
  if (size < 6) return;

  const int16_t x = canvas.width() / 2 - size / 2;
  const int16_t y = kMarkY + (kMarkSize - size) / 2;
  const uint16_t background = theme::kBackdrop;

  theme::glow(canvas, x, y, size, size, size / 3, theme::kPrimary, background, 3);
  theme::gradientPill(canvas, x, y, size, size, theme::kPrimary,
                      theme::kSecondary);
  // Inner glass bead, so the mark reads as an object rather than a swatch.
  const int16_t bead = size / 3;
  theme::sphere(canvas, x + size / 2, y + size / 2, bead,
                theme::mix(theme::kSecondary, theme::kWhite, 0.25f),
                theme::kTertiary,
                theme::mix(theme::kPrimary, theme::kSecondary, 0.5f), 1.2f);
}

void WelcomeAnimation::drawWordmark(TFT_eSprite& canvas, float progress) {
  const int16_t centerX = canvas.width() / 2;
  const float nameFade = theme::easeOutCubic(phase(progress, kNameStart, kNameEnd));
  if (nameFade > 0.0f) {
    const uint16_t background = theme::kBackdrop;
    const int16_t slide = static_cast<int16_t>((1.0f - nameFade) * 22.0f);
    const int16_t width = canvas.textWidth(BoardConfig::kProductName, 4);
    const int16_t x = centerX - width / 2 - slide;
    canvas.setTextColor(theme::mix(background, theme::kBevelDark, nameFade));
    canvas.drawString(BoardConfig::kProductName, x + 2, kWordmarkY + 2, 4);
    canvas.setTextColor(theme::mix(background, theme::kText, nameFade));
    canvas.drawString(BoardConfig::kProductName, x, kWordmarkY, 4);
  }

  const float suffixFade =
      theme::easeOutCubic(phase(progress, kSuffixStart, kSuffixEnd));
  if (suffixFade > 0.0f) {
    drawSpacedText(canvas, BoardConfig::kProductSuffix, centerX, kSuffixY, 1,
                   kLetterGap,
                   theme::mix(theme::kBackdrop, theme::kTertiary, suffixFade));
  }
}

void WelcomeAnimation::drawShine(TFT_eSprite& canvas, float progress) {
  const float sweep = phase(progress, kShineStart, kShineEnd);
  if (sweep <= 0.0f || sweep >= 1.0f) return;
  theme::sheenSweep(canvas, 0, kMarkY - 4, canvas.width(), kSuffixY - kMarkY + 20,
                    sweep, 0.34f);
}

void WelcomeAnimation::drawFooter(TFT_eSprite& canvas, float progress) {
  const int16_t width = 150;
  liquid::bar(canvas, canvas.width() / 2 - width / 2, kBarY, width, 14,
              theme::smoothStep(theme::clamp01(progress / 0.94f)), millis(),
              theme::kPrimary, theme::kTertiary, theme::kBackdrop);
}

bool WelcomeAnimation::tick(TFT_eSprite& canvas, int16_t originX, int16_t originY,
                            uint32_t now) {
  if (!active_) return false;
  const uint32_t elapsed = now - started_;
  const float progress =
      theme::clamp01(static_cast<float>(elapsed) / BoardConfig::kWelcomeMs);

  for (int16_t row = 0; row < canvas.height(); ++row) {
    canvas.drawFastHLine(0, row, canvas.width(), rowBackground(originY, row));
  }

  drawOrb(canvas, progress);
  drawWordmark(canvas, progress);
  drawShine(canvas, progress);
  drawFooter(canvas, progress);
  canvas.pushSprite(originX, originY);

  if (elapsed >= BoardConfig::kWelcomeMs) active_ = false;
  return active_;
}
