#include "test_pattern.h"
#include "version.h"
#define MWC_VERSION_TEXT ("v" MWC_VERSION)

namespace test_pattern {
  void draw(Canvas& c, uint32_t elapsed_ms, const char* line1, const char* line2) {
    const int16_t W = c.width(), H = c.height();
    elapsed_ms %= DURATION_MS;
    c.fillScreen(0);
    if (elapsed_ms < 1600) {
      static const uint16_t solid[4] = { 0xF800, 0x07E0, 0x001F, 0xFFFF };
      c.fillScreen(solid[elapsed_ms / 400]);
    } else if (elapsed_ms < 4000) {
      c.drawRect(0, 0, W, H, 0xFFFF);
      c.fillRect(1, 1, 2, 2, 0xF800);             // top-left red
      c.fillRect(W - 3, 1, 2, 2, 0x07E0);         // top-right green
      c.fillRect(1, H - 3, 2, 2, 0x001F);         // bottom-left blue
      c.fillRect(W - 3, H - 3, 2, 2, 0xFFE0);     // bottom-right yellow
      c.drawLine(0, 0, W - 1, H - 1, 0x7BEF);     // diagonal shows scan/row order problems
      for (int16_t y = 4; y < H - 4; y += 4) c.drawFastHLine(W / 2 - 3, y, 7, (y / 4) & 1 ? 0xF800 : 0x001F);
    } else if (elapsed_ms < 7000) {
      const int16_t barH = H / 4;
      for (int16_t x = 0; x < W; x++) {
        uint8_t v = (uint8_t)((x * 255) / (W - 1));
        c.drawFastVLine(x, 0 * barH, barH, Canvas::color565(v, 0, 0));
        c.drawFastVLine(x, 1 * barH, barH, Canvas::color565(0, v, 0));
        c.drawFastVLine(x, 2 * barH, barH, Canvas::color565(0, 0, v));
        c.drawFastVLine(x, 3 * barH, H - 3 * barH, Canvas::color565(v, v, v));
      }
    } else {
      c.setFont(nullptr);
      c.setTextSize(1);
      c.setTextWrap(false);
      c.drawText(line1 ? line1 : "", 1, 2, 0xFFFF);
      c.drawText(line2 ? line2 : "", 1, 12, 0x07FF);
      c.drawText(MWC_VERSION_TEXT, 1, 22, 0x7BEF);
    }
  }
}
