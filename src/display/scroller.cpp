#include "scroller.h"

void Scroller::setText(const char* t, uint8_t px_per_s, uint32_t now_ms) {
  if (text == t && speed == px_per_s) return;
  text = t;
  speed = px_per_s ? px_per_s : 1;
  startMs = now_ms;
  measured = false;
}

void Scroller::draw(Canvas& c, int16_t x, int16_t y, int16_t w, uint16_t color, uint32_t now_ms, bool centerIfFits) {
  if (text.length() == 0) return;
  if (!measured) { textW = c.textWidth(text.c_str()); measured = true; }
  c.setTextWrap(false);
  if (textW <= w) {
    int16_t tx = centerIfFits ? x + (w - textW) / 2 : x;
    c.drawText(text.c_str(), tx, y, color);
    return;
  }
  const int32_t span = textW + w;                       // enter from the right, leave to the left
  int32_t off = (int32_t)(((uint64_t)(now_ms - startMs) * speed / 1000) % span);
  c.drawText(text.c_str(), x + w - off, y, color);
  // clear whatever spilled outside the window
  int16_t th = c.textHeight(text.c_str()) + 2;
  if (x > 0) c.fillRect(0, y, x, th, 0);
  if (x + w < c.width()) c.fillRect(x + w, y, c.width() - (x + w), th, 0);
}
