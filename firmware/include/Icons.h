#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

// Small vector icons drawn procedurally, sized by their bounding box so the
// same call works for a 16 px row icon or a 24 px tile.
namespace icons {

// Processor die with pins on all four sides.
void cpu(TFT_eSPI& g, int16_t x, int16_t y, int16_t size, uint16_t color,
         uint16_t accent);
// Graphics card: board outline with a fan.
void gpu(TFT_eSPI& g, int16_t x, int16_t y, int16_t size, uint16_t color,
         uint16_t accent);
// Memory module: stick with contact teeth and a notch.
void memory(TFT_eSPI& g, int16_t x, int16_t y, int16_t size, uint16_t color,
            uint16_t accent);
// Wi-Fi/link arcs, filled according to `bars` (0-3).
void link(TFT_eSPI& g, int16_t x, int16_t y, int16_t size, uint16_t color,
          uint16_t dim, uint8_t bars);
// Power symbol.
void power(TFT_eSPI& g, int16_t x, int16_t y, int16_t size, uint16_t color);
// Beamed quaver, used on the now-playing card.
void note(TFT_eSPI& g, int16_t x, int16_t y, int16_t size, uint16_t color,
          uint16_t accent);
// Transport glyph: filled triangle when playing, two bars when paused.
void transport(TFT_eSPI& g, int16_t x, int16_t y, int16_t size, uint16_t color,
               bool playing);
// Speaker with waves; `waves` (0-3) follows the level, 0 draws a mute cross.
void speaker(TFT_eSPI& g, int16_t x, int16_t y, int16_t size, uint16_t color,
             uint16_t dim, uint8_t waves, bool muted);

}  // namespace icons
