#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

// Animated liquid fill bar: gradient body with a bright leading edge and a
// field of small bubbles rising through it towards the surface.
namespace liquid {

// progress is 0..1. `now` drives the bubbles, so the bar keeps moving even when
// the value is steady.
void bar(TFT_eSPI& g, int16_t x, int16_t y, int16_t w, int16_t h, float progress,
         uint32_t now, uint16_t from, uint16_t to, uint16_t background);


}  // namespace liquid
