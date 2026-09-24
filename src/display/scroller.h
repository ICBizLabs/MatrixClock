#pragma once
#include <Arduino.h>
#include "canvas.h"

// Horizontally scrolling text inside a clip window. Text that fits is drawn statically.
class Scroller {
 public:
  void setText(const char* t, uint8_t px_per_s, uint32_t now_ms);   // no-op when the text is unchanged
  void restart(uint32_t now_ms) { startMs = now_ms; }
  bool empty() const { return text.length() == 0; }
  // Draws into the canvas using its current font. Everything outside [x, x+w) is cleared to black afterwards,
  // so draw neighbouring content after calling this.
  void draw(Canvas& c, int16_t x, int16_t y, int16_t w, uint16_t color, uint32_t now_ms, bool centerIfFits = false);

 private:
  String text;
  int16_t textW = 0;
  uint8_t speed = 20;
  uint32_t startMs = 0;
  bool measured = false;
};
