#include "Icons.h"

#include "DisplayTheme.h"

namespace icons {
namespace {
// TFT_eSPI arc angles start at 6 o'clock and increase clockwise, so an upward
// fan spans 120 to 240.
constexpr uint32_t kFanStart = 120;
constexpr uint32_t kFanEnd = 240;

int16_t atLeast(int16_t value, int16_t minimum) {
  return value < minimum ? minimum : value;
}

// Shadow tone for a glyph. Icons sit on bright gradient tiles, so a plain dark
// offset copy is what separates them from the background.
uint16_t shadowOf(uint16_t color) {
  return theme::mix(color, theme::kBevelDark, 0.72f);
}
}  // namespace

void cpu(TFT_eSPI& g, int16_t cx, int16_t cy, int16_t size, uint16_t color,
         uint16_t accent) {
  const int16_t pin = atLeast(size / 6, 2);
  const int16_t body = size - pin * 2;
  const int16_t die = atLeast(body / 2, 4);

  // Drawn twice: once offset as a shadow, once lit.
  for (uint8_t pass = 0; pass < 2; ++pass) {
    const int16_t ox = cx - size / 2 + (pass == 0 ? 1 : 0);
    const int16_t oy = cy - size / 2 + (pass == 0 ? 1 : 0);
    const uint16_t line = pass == 0 ? shadowOf(color) : color;
    const uint16_t core = pass == 0 ? shadowOf(color) : accent;
    const int16_t bodyX = ox + pin;
    const int16_t bodyY = oy + pin;

    // Three pins per side, evenly spaced across the body.
    for (uint8_t i = 0; i < 3; ++i) {
      const int16_t along = bodyX + body * (i * 2 + 1) / 6;
      g.drawFastVLine(along, oy, pin, line);
      g.drawFastVLine(along, oy + size - pin, pin, line);
      const int16_t down = bodyY + body * (i * 2 + 1) / 6;
      g.drawFastHLine(ox, down, pin, line);
      g.drawFastHLine(ox + size - pin, down, pin, line);
    }

    g.drawRoundRect(bodyX, bodyY, body, body, 3, line);
    // Filled die with a darker window, so the chip reads as layered.
    g.fillRoundRect(bodyX + (body - die) / 2, bodyY + (body - die) / 2, die, die,
                    2, core);
    if (pass == 1 && die >= 6) {
      g.drawRoundRect(bodyX + (body - die) / 2 + 1, bodyY + (body - die) / 2 + 1,
                      die - 2, die - 2, 1, theme::lighten(accent, 0.45f));
    }
  }
}

void gpu(TFT_eSPI& g, int16_t cx, int16_t cy, int16_t size, uint16_t color,
         uint16_t accent) {
  const int16_t height = size * 3 / 4;
  const int16_t fanR = atLeast(height / 3, 3);

  for (uint8_t pass = 0; pass < 2; ++pass) {
    const int16_t ox = cx - size / 2 + (pass == 0 ? 1 : 0);
    const int16_t oy = cy - height / 2 + (pass == 0 ? 1 : 0);
    const uint16_t line = pass == 0 ? shadowOf(color) : color;
    const uint16_t blade = pass == 0 ? shadowOf(color) : accent;

    g.drawRoundRect(ox, oy, size, height, 2, line);
    // Bracket down the left edge and heatsink fins along the top.
    g.drawFastVLine(ox + 2, oy + 2, height - 4, line);
    for (int16_t fin = ox + 5; fin < ox + size - fanR * 2 - 3; fin += 3) {
      g.drawFastVLine(fin, oy + 3, height - 6, theme::mix(line, color, 0.35f));
    }

    const int16_t fanX = ox + size - fanR - 3;
    const int16_t fanY = oy + height / 2;
    g.fillCircle(fanX, fanY, fanR, blade);
    if (pass == 1) {
      // Blades as spokes through the hub.
      g.drawFastHLine(fanX - fanR, fanY, fanR * 2 + 1, theme::darken(accent, 0.5f));
      g.drawFastVLine(fanX, fanY - fanR, fanR * 2 + 1, theme::darken(accent, 0.5f));
      g.fillCircle(fanX, fanY, 1, color);
    }
  }
}

void memory(TFT_eSPI& g, int16_t cx, int16_t cy, int16_t size, uint16_t color,
            uint16_t accent) {
  const int16_t height = size * 3 / 4;
  const int16_t teeth = atLeast(height / 5, 2);
  const int16_t boardH = height - teeth;
  const int16_t chipW = atLeast(size / 6, 3);

  for (uint8_t pass = 0; pass < 2; ++pass) {
    const int16_t ox = cx - size / 2 + (pass == 0 ? 1 : 0);
    const int16_t oy = cy - height / 2 + (pass == 0 ? 1 : 0);
    const uint16_t line = pass == 0 ? shadowOf(color) : color;
    const uint16_t chip = pass == 0 ? shadowOf(color) : accent;

    g.drawRoundRect(ox, oy, size, boardH, 2, line);
    // Three memory chips across the board, the middle one highlighted.
    for (uint8_t i = 0; i < 3; ++i) {
      const int16_t chipX = ox + 3 + i * (chipW + 2);
      if (chipX + chipW > ox + size - 3) break;
      g.fillRect(chipX, oy + 3, chipW, boardH - 6, i == 1 ? chip : line);
    }
    // Contact teeth with the keying notch.
    for (int16_t column = ox + 2; column < ox + size - 2; column += 3) {
      if (column > ox + size / 2 - 3 && column < ox + size / 2 + 2) continue;
      g.drawFastVLine(column, oy + boardH, teeth, line);
    }
  }
}

void link(TFT_eSPI& g, int16_t cx, int16_t cy, int16_t size, uint16_t color,
          uint16_t dim, uint8_t bars) {
  // The fan grows upward from its origin, so the origin sits below centre for
  // the icon as a whole to look centred.
  const int16_t base = cy + size / 2 - 1;
  const uint16_t background = theme::tone(theme::kInk);
  g.fillCircle(cx, base, 1, bars > 0 ? color : dim);
  for (uint8_t arc = 1; arc <= 3; ++arc) {
    const int16_t r = size * arc / 4;
    g.drawSmoothArc(cx, base, r, r - 1, kFanStart, kFanEnd,
                    arc <= bars ? color : dim, background, false);
  }
}

void power(TFT_eSPI& g, int16_t cx, int16_t cy, int16_t size, uint16_t color) {
  const int16_t r = size / 2 - 1;
  // Wraps past 6 o'clock so the break sits at the top, under the stem.
  g.drawSmoothArc(cx, cy, r, r - 2, 215, 145, color, theme::tone(theme::kInk),
                  true);
  g.fillRect(cx - 1, cy - size / 2, 2, size / 2 - 1, color);
}

void note(TFT_eSPI& g, int16_t cx, int16_t cy, int16_t size, uint16_t color,
          uint16_t accent) {
  const int16_t headR = atLeast(size / 5, 2);
  const int16_t left = cx - size / 2;
  const int16_t top = cy - size / 2;
  const int16_t stemTop = top + 1;
  const int16_t stemBottom = top + size - headR - 1;
  const int16_t leftStem = left + headR;
  const int16_t rightStem = left + size - headR - 1;

  g.drawFastVLine(leftStem, stemTop, stemBottom - stemTop, color);
  g.drawFastVLine(rightStem, stemTop, stemBottom - stemTop, color);
  g.fillRect(leftStem, stemTop, rightStem - leftStem + 1, atLeast(size / 8, 2),
             accent);
  g.fillCircle(leftStem - headR + 1, stemBottom, headR, color);
  g.fillCircle(rightStem - headR + 1, stemBottom, headR, color);
}

void transport(TFT_eSPI& g, int16_t cx, int16_t cy, int16_t size, uint16_t color,
               bool playing) {
  const int16_t half = size / 2;
  if (playing) {
    // Optically centred: a triangle's mass sits left of its bounding box.
    g.fillTriangle(cx - half + 1, cy - half, cx - half + 1, cy + half,
                   cx + half - 1, cy, color);
    return;
  }
  const int16_t bar = atLeast(size / 3, 2);
  g.fillRect(cx - half, cy - half, bar, size, color);
  g.fillRect(cx + half - bar, cy - half, bar, size, color);
}

void speaker(TFT_eSPI& g, int16_t cx, int16_t cy, int16_t size, uint16_t color,
             uint16_t dim, uint8_t waves, bool muted) {
  const int16_t bodyH = size / 2;
  const int16_t coneW = size / 3;
  // The waves live to the right of the cone, so the cone starts left of centre.
  const int16_t left = cx - size / 2;

  g.fillRect(left, cy - bodyH / 2, coneW - 2, bodyH, color);
  g.fillTriangle(left + coneW - 2, cy - bodyH / 2, left + coneW - 2,
                 cy + bodyH / 2 - 1, left + coneW + 2, cy - size / 2, color);
  g.fillTriangle(left + coneW - 2, cy + bodyH / 2 - 1, left + coneW + 2,
                 cy + size / 2, left + coneW + 2, cy - size / 2, color);

  if (muted) {
    const int16_t crossX = left + coneW + 5;
    g.drawLine(crossX, cy - 3, crossX + 6, cy + 3, color);
    g.drawLine(crossX, cy + 3, crossX + 6, cy - 3, color);
    return;
  }
  for (uint8_t wave = 1; wave <= 3; ++wave) {
    const int16_t r = coneW + wave * 3;
    g.drawSmoothArc(left + coneW - 1, cy, r, r - 1, 210, 330,
                    wave <= waves ? color : dim, theme::tone(theme::kInk),
                    false);
  }
}

}  // namespace icons
