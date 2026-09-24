#include "canvas.h"

int16_t Canvas::textWidth(const char* s) {
  int16_t x1, y1; uint16_t w, h;
  getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  return (int16_t)w;
}

int16_t Canvas::textHeight(const char* s) {
  int16_t x1, y1; uint16_t w, h;
  getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  return (int16_t)h;
}

void Canvas::drawText(const char* s, int16_t x, int16_t y, uint16_t color) {
  // For GFX fonts the cursor is the baseline; getTextBounds tells us how far above the baseline glyphs extend.
  int16_t x1, y1; uint16_t w, h;
  getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  setTextColor(color);
  setCursor(x - x1, y - y1);
  print(s);
}

void Canvas::drawTextCentered(const char* s, int16_t cx, int16_t y, uint16_t color) {
  int16_t x = cx - textWidth(s) / 2;
  drawText(s, x < 0 ? 0 : x, y, color);   // wide text is clipped on the right rather than losing its first letter
}

void Canvas::drawTextRight(const char* s, int16_t rx, int16_t y, uint16_t color) {
  drawText(s, rx - textWidth(s), y, color);
}
