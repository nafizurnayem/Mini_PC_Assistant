#include "Icons.h"

#include "DisplayTheme.h"

namespace icons {
namespace {
// TFT_eSPI arc angles start at 6 o'clock and increase clockwise, so an upward
// fan spans 120 to 240 and a gap at the top means wrapping past 0.
constexpr uint32_t kFanStart = 120;
constexpr uint32_t kFanEnd = 240;

int16_t atLeast(int16_t value, int16_t minimum) {
  return value < minimum ? minimum : value;
}
}  // namespace

void cpu(TFT_eSPI& g, int16_t x, int16_t y, int16_t size, uint16_t color,
         uint16_t accent) {
  const int16_t pin = atLeast(size / 6, 2);
  const int16_t bodyX = x + pin;
  const int16_t bodyY = y + pin;
  const int16_t body = size - pin * 2;

  // Pins: three per side.
  for (uint8_t i = 0; i < 3; ++i) {
    const int16_t offset = bodyX + body * (i * 2 + 1) / 6;
    g.drawFastVLine(offset, y, pin, color);
    g.drawFastVLine(offset, y + size - pin, pin, color);
    const int16_t vertical = bodyY + body * (i * 2 + 1) / 6;
    g.drawFastHLine(x, vertical, pin, color);
    g.drawFastHLine(x + size - pin, vertical, pin, color);
  }

  g.drawRoundRect(bodyX, bodyY, body, body, 2, color);
  const int16_t die = atLeast(body / 3, 3);
  g.fillRoundRect(bodyX + (body - die) / 2, bodyY + (body - die) / 2, die, die, 1,
                  accent);
}

void gpu(TFT_eSPI& g, int16_t x, int16_t y, int16_t size, uint16_t color,
         uint16_t accent) {
  const int16_t height = size * 3 / 4;
  const int16_t top = y + (size - height) / 2;
  g.drawRoundRect(x, top, size, height, 2, color);
  // Bracket edge.
  g.drawFastVLine(x + 2, top + 2, height - 4, color);

  const int16_t fanR = atLeast(height / 3, 3);
  const int16_t fanX = x + size - fanR - 3;
  const int16_t fanY = top + height / 2;
  g.drawCircle(fanX, fanY, fanR, accent);
  g.drawFastHLine(fanX - fanR + 1, fanY, fanR * 2 - 1, accent);
  g.drawFastVLine(fanX, fanY - fanR + 1, fanR * 2 - 1, accent);
  g.drawPixel(fanX, fanY, color);
}

void memory(TFT_eSPI& g, int16_t x, int16_t y, int16_t size, uint16_t color,
            uint16_t accent) {
  const int16_t height = size * 3 / 4;
  const int16_t top = y + (size - height) / 2;
  const int16_t teeth = atLeast(height / 5, 2);

  g.drawRoundRect(x, top, size, height - teeth, 2, color);
  // Chips on the board.
  const int16_t chipW = atLeast(size / 5, 2);
  for (uint8_t i = 0; i < 3; ++i) {
    g.fillRect(x + 3 + i * (chipW + 2), top + 3, chipW, height - teeth - 6,
               i == 1 ? accent : color);
  }
  // Contact teeth with the keying notch.
  for (int16_t column = x + 1; column < x + size - 1; column += 3) {
    if (column > x + size / 2 - 2 && column < x + size / 2 + 2) continue;
    g.drawFastVLine(column, top + height - teeth, teeth, color);
  }
}


void link(TFT_eSPI& g, int16_t x, int16_t y, int16_t size, uint16_t color,
          uint16_t dim, uint8_t bars) {
  const int16_t cx = x + size / 2;
  const int16_t base = y + size - 2;
  g.fillCircle(cx, base, 1, bars > 0 ? color : dim);
  for (uint8_t arc = 1; arc <= 3; ++arc) {
    const int16_t r = size * arc / 4;
    g.drawSmoothArc(cx, base, r, r - 1, kFanStart, kFanEnd,
                    arc <= bars ? color : dim, theme::tone(theme::kInk), false);
  }
}

void power(TFT_eSPI& g, int16_t x, int16_t y, int16_t size, uint16_t color) {
  const int16_t cx = x + size / 2;
  const int16_t cy = y + size / 2;
  const int16_t r = size / 2 - 1;
  // Wraps past 6 o'clock so the break sits at the top, under the stem.
  g.drawSmoothArc(cx, cy, r, r - 2, 215, 145, color, theme::tone(theme::kInk),
                  true);
  g.fillRect(cx - 1, y + 1, 2, size / 2 - 1, color);
}

void note(TFT_eSPI& g, int16_t x, int16_t y, int16_t size, uint16_t color,
          uint16_t accent) {
  const int16_t headR = atLeast(size / 5, 2);
  const int16_t stemTop = y + 1;
  const int16_t stemBottom = y + size - headR - 1;
  const int16_t leftStem = x + headR;
  const int16_t rightStem = x + size - headR - 1;

  g.drawFastVLine(leftStem, stemTop, stemBottom - stemTop, color);
  g.drawFastVLine(rightStem, stemTop, stemBottom - stemTop, color);
  // Beam joining the two stems.
  g.fillRect(leftStem, stemTop, rightStem - leftStem + 1, atLeast(size / 8, 2),
             accent);
  g.fillCircle(leftStem - headR + 1, stemBottom, headR, color);
  g.fillCircle(rightStem - headR + 1, stemBottom, headR, color);
}

void transport(TFT_eSPI& g, int16_t x, int16_t y, int16_t size, uint16_t color,
               bool playing) {
  if (playing) {
    g.fillTriangle(x + 1, y, x + 1, y + size, x + size, y + size / 2, color);
    return;
  }
  const int16_t bar = atLeast(size / 3, 2);
  g.fillRect(x, y, bar, size, color);
  g.fillRect(x + size - bar, y, bar, size, color);
}

void speaker(TFT_eSPI& g, int16_t x, int16_t y, int16_t size, uint16_t color,
             uint16_t dim, uint8_t waves, bool muted) {
  const int16_t bodyH = size / 2;
  const int16_t cy = y + size / 2;
  const int16_t coneW = size / 3;

  g.fillRect(x, cy - bodyH / 2, coneW - 2, bodyH, color);
  g.fillTriangle(x + coneW - 2, cy - bodyH / 2, x + coneW - 2, cy + bodyH / 2 - 1,
                 x + coneW + 2, cy - size / 2, color);
  g.fillTriangle(x + coneW - 2, cy + bodyH / 2 - 1, x + coneW + 2, cy + size / 2,
                 x + coneW + 2, cy - size / 2, color);

  if (muted) {
    const int16_t cx = x + coneW + 5;
    g.drawLine(cx, cy - 3, cx + 6, cy + 3, color);
    g.drawLine(cx, cy + 3, cx + 6, cy - 3, color);
    return;
  }
  for (uint8_t wave = 1; wave <= 3; ++wave) {
    const int16_t r = coneW + wave * 3;
    g.drawSmoothArc(x + coneW - 1, cy, r, r - 1, 210, 330,
                    wave <= waves ? color : dim, theme::tone(theme::kInk),
                    false);
  }
}

}  // namespace icons
