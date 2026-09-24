#pragma once
#include <Arduino.h>
#include "canvas.h"

// Non-blocking panel test pattern. Draws the phase for the given elapsed time into the canvas.
// Phases: solid R/G/B/W, border + corner markers, colour gradients, info text. Total TEST_PATTERN_MS.
namespace test_pattern {
  constexpr uint32_t DURATION_MS = 10000;
  void draw(Canvas& c, uint32_t elapsed_ms, const char* line1, const char* line2);
}
