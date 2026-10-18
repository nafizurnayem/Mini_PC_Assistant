#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

// Small vector icons. Every icon draws around a centre point rather than from a
// corner: the shapes have different proportions inside their box, so anchoring
// by corner left them visibly off-centre inside a tile.
//
// `size` is the width of the icon's bounding box. Each icon paints a soft dark
// shadow first, which is what keeps a white glyph readable on a bright tile.
namespace icons {

void cpu(TFT_eSPI& g, int16_t cx, int16_t cy, int16_t size, uint16_t color,
         uint16_t accent);
void gpu(TFT_eSPI& g, int16_t cx, int16_t cy, int16_t size, uint16_t color,
         uint16_t accent);
void memory(TFT_eSPI& g, int16_t cx, int16_t cy, int16_t size, uint16_t color,
            uint16_t accent);
// Wi-Fi arcs, filled according to `bars` (0-3).
void link(TFT_eSPI& g, int16_t cx, int16_t cy, int16_t size, uint16_t color,
          uint16_t dim, uint8_t bars);
void power(TFT_eSPI& g, int16_t cx, int16_t cy, int16_t size, uint16_t color);
void note(TFT_eSPI& g, int16_t cx, int16_t cy, int16_t size, uint16_t color,
          uint16_t accent);
// Filled triangle when playing, two bars when paused.
void transport(TFT_eSPI& g, int16_t cx, int16_t cy, int16_t size, uint16_t color,
               bool playing);
// Speaker with waves; `waves` (0-3) follows the level, muted draws a cross.
void speaker(TFT_eSPI& g, int16_t cx, int16_t cy, int16_t size, uint16_t color,
             uint16_t dim, uint8_t waves, bool muted);

}  // namespace icons
