#pragma once
#include <Adafruit_GFX.h>

// WMO weather interpretation codes (as delivered by Open-Meteo) -> icon + short text, and procedural 16x16 icons.
namespace wmo {
  enum Icon : uint8_t { SUN, MOON, PARTLY_DAY, PARTLY_NIGHT, CLOUDY, FOG, DRIZZLE, RAIN, SLEET, SNOW, THUNDER, THUNDER_HAIL, UNKNOWN };
  Icon icon(uint8_t code, bool is_day);
  const char* text(uint8_t code);                     // short condition text
  void drawIcon(Adafruit_GFX& g, int16_t x, int16_t y, Icon ic);   // 16x16 cell with top-left at (x, y)
}
