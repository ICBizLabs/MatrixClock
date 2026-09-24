#pragma once
#include <stdint.h>

// Every screen coordinate, font choice and timing for the 64x32 layout lives here so it can be tuned in one place.
namespace layout {
  constexpr int16_t W = 64, H = 32;

  // Top half: the clock
  constexpr int16_t TIME_Y = 2;              // top row of the 7x11 digits (rows 2..12)
  constexpr int16_t TIME_X_CENTERED = 14;    // x of the first digit cell when nothing is drawn at the right
  constexpr int16_t TIME_X_LEFT = 4;         // x of the first digit cell when AM/PM or seconds are shown
  constexpr int16_t ANNOT_X = 45;            // AM/PM and seconds column (3x5 font)
  constexpr int16_t AMPM_Y = 3, SECS_Y = 10; // top rows of the annotation glyphs
  constexpr int16_t STATUS_Y = 16;           // status line when no time is known

  // Bottom half: pages and banner
  constexpr int16_t BOTTOM_Y = 16, BOTTOM_H = 16;
  constexpr int16_t ICON_X = 0, ICON_Y = 16, ICON_SIZE = 16;
  constexpr int16_t TEXT_X = 18;             // text start when an icon is shown
  constexpr int16_t LINE1_Y = 17, LINE2_Y = 25;   // two 5x7 text lines in the bottom half
  constexpr int16_t SINGLE_Y = 20;           // one vertically centred 5x7 line
  constexpr int16_t BANNER_Y = 20;

  // Forecast screen: three 21-px columns
  constexpr int16_t FC_COL_W = 21;
  constexpr int16_t FC_DAY_Y = 0, FC_ICON_Y = 6, FC_TEMP_Y = 27;

  constexpr uint16_t COLON_PERIOD_MS = 1000;
  constexpr uint16_t FRAME_FLASH_MS = 500;
  constexpr uint16_t SPLASH_MS = 1500;
}
