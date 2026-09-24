#pragma once
#include <Adafruit_GFX.h>

// 16-bit off-screen canvas (DRAM) with a few text helpers. panel::present() diffs it against the last frame.
class Canvas : public GFXcanvas16 {
 public:
  Canvas(uint16_t w, uint16_t h) : GFXcanvas16(w, h) {}

  int16_t textWidth(const char* s);
  int16_t textHeight(const char* s);
  // Draws s with its top-left at (x, y) regardless of font type (classic 5x7 or GFX font)
  void drawText(const char* s, int16_t x, int16_t y, uint16_t color);
  void drawTextCentered(const char* s, int16_t cx, int16_t y, uint16_t color);
  void drawTextRight(const char* s, int16_t rx, int16_t y, uint16_t color);
  static uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)); }
  static uint16_t rgb(uint32_t rgb888) { return color565((rgb888 >> 16) & 0xFF, (rgb888 >> 8) & 0xFF, rgb888 & 0xFF); }
};
