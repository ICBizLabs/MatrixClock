#include "wmo.h"

namespace wmo {
  namespace {
    constexpr uint16_t C_SUN = 0xFE60, C_MOON = 0xDEFB, C_CLOUD = 0xBDF7, C_DARK = 0x6B6D, C_RAIN = 0x3D7F,
                       C_SNOW = 0xFFFF, C_BOLT = 0xFFE0, C_FOG = 0x9CD3, C_BLACK = 0x0000;

    // Rounded cloud filling roughly x+1..x+14, y+ty..y+ty+7
    void cloud(Adafruit_GFX& g, int16_t x, int16_t y, int16_t ty, uint16_t c) {
      g.fillCircle(x + 5, y + ty + 4, 3, c);
      g.fillCircle(x + 10, y + ty + 3, 4, c);
      g.fillRect(x + 2, y + ty + 4, 12, 4, c);
    }
    void sun(Adafruit_GFX& g, int16_t cx, int16_t cy, int16_t r, uint16_t c) {
      g.fillCircle(cx, cy, r, c);
      g.drawFastHLine(cx - r - 3, cy, 2, c); g.drawFastHLine(cx + r + 2, cy, 2, c);
      g.drawFastVLine(cx, cy - r - 3, 2, c); g.drawFastVLine(cx, cy + r + 2, 2, c);
      g.drawPixel(cx - r - 1, cy - r - 1, c); g.drawPixel(cx + r + 1, cy - r - 1, c);
      g.drawPixel(cx - r - 1, cy + r + 1, c); g.drawPixel(cx + r + 1, cy + r + 1, c);
    }
    void moon(Adafruit_GFX& g, int16_t cx, int16_t cy, int16_t r) {
      g.fillCircle(cx, cy, r, C_MOON);
      g.fillCircle(cx + r / 2 + 1, cy - r / 2, r, C_BLACK);
    }
    void rainDrops(Adafruit_GFX& g, int16_t x, int16_t y, uint16_t c, bool lines) {
      for (int i = 0; i < 3; i++) {
        int16_t dx = x + 4 + i * 4, dy = y + 12 + (i & 1);
        if (lines) g.drawFastVLine(dx, dy, 3, c); else g.drawPixel(dx, dy + 1, c);
      }
    }
    void snowFlakes(Adafruit_GFX& g, int16_t x, int16_t y) {
      for (int i = 0; i < 3; i++) {
        int16_t dx = x + 4 + i * 4, dy = y + 13 + (i & 1);
        g.drawPixel(dx, dy, C_SNOW); g.drawPixel(dx - 1, dy, C_SNOW); g.drawPixel(dx + 1, dy, C_SNOW);
        g.drawPixel(dx, dy - 1, C_SNOW); g.drawPixel(dx, dy + 1, C_SNOW);
      }
    }
    void bolt(Adafruit_GFX& g, int16_t x, int16_t y) {
      g.drawLine(x + 8, y + 9, x + 6, y + 12, C_BOLT);
      g.drawLine(x + 6, y + 12, x + 9, y + 12, C_BOLT);
      g.drawLine(x + 9, y + 12, x + 7, y + 15, C_BOLT);
    }
  }

  Icon icon(uint8_t code, bool is_day) {
    switch (code) {
      case 0: case 1: return is_day ? SUN : MOON;
      case 2: return is_day ? PARTLY_DAY : PARTLY_NIGHT;
      case 3: return CLOUDY;
      case 45: case 48: return FOG;
      case 51: case 53: case 55: return DRIZZLE;
      case 56: case 57: case 66: case 67: return SLEET;
      case 61: case 63: case 65: case 80: case 81: case 82: return RAIN;
      case 71: case 73: case 75: case 77: case 85: case 86: return SNOW;
      case 95: return THUNDER;
      case 96: case 99: return THUNDER_HAIL;
      default: return UNKNOWN;
    }
  }

  const char* text(uint8_t code) {
    switch (code) {
      case 0: return "Clear";
      case 1: return "Mostly clear";
      case 2: return "Partly cloudy";
      case 3: return "Overcast";
      case 45: return "Fog";
      case 48: return "Icy fog";
      case 51: return "Lt drizzle";
      case 53: return "Drizzle";
      case 55: return "Hvy drizzle";
      case 56: case 57: return "Frz drizzle";
      case 61: return "Lt rain";
      case 63: return "Rain";
      case 65: return "Heavy rain";
      case 66: case 67: return "Frz rain";
      case 71: return "Lt snow";
      case 73: return "Snow";
      case 75: return "Heavy snow";
      case 77: return "Snow grains";
      case 80: return "Lt showers";
      case 81: return "Showers";
      case 82: return "Hvy showers";
      case 85: return "Snow showers";
      case 86: return "Hvy snow shwr";
      case 95: return "T-storm";
      case 96: case 99: return "T-storm hail";
      default: return "Unknown";
    }
  }

  void drawIcon(Adafruit_GFX& g, int16_t x, int16_t y, Icon ic) {
    switch (ic) {
      case SUN: sun(g, x + 8, y + 8, 3, C_SUN); break;
      case MOON: moon(g, x + 7, y + 8, 5); break;
      case PARTLY_DAY: sun(g, x + 5, y + 5, 2, C_SUN); cloud(g, x, y, 5, C_CLOUD); break;
      case PARTLY_NIGHT: moon(g, x + 5, y + 5, 3); cloud(g, x, y, 5, C_CLOUD); break;
      case CLOUDY: cloud(g, x, y, 2, C_DARK); cloud(g, x, y, 6, C_CLOUD); break;
      case FOG:
        cloud(g, x, y, 1, C_CLOUD);
        for (int i = 0; i < 3; i++) g.drawFastHLine(x + 2 + (i & 1) * 2, y + 10 + i * 2, 11, C_FOG);
        break;
      case DRIZZLE: cloud(g, x, y, 2, C_CLOUD); rainDrops(g, x, y, C_RAIN, false); break;
      case RAIN: cloud(g, x, y, 2, C_DARK); rainDrops(g, x, y, C_RAIN, true); break;
      case SLEET: cloud(g, x, y, 2, C_CLOUD); rainDrops(g, x, y, C_RAIN, true); g.drawPixel(x + 8, y + 15, C_SNOW); g.drawPixel(x + 4, y + 15, C_SNOW); break;
      case SNOW: cloud(g, x, y, 2, C_CLOUD); snowFlakes(g, x, y); break;
      case THUNDER: cloud(g, x, y, 1, C_DARK); bolt(g, x, y); break;
      case THUNDER_HAIL: cloud(g, x, y, 1, C_DARK); bolt(g, x, y); g.drawPixel(x + 3, y + 13, C_SNOW); g.drawPixel(x + 12, y + 14, C_SNOW); break;
      default:
        g.setFont(nullptr); g.setTextSize(1); g.setTextColor(C_FOG); g.setCursor(x + 5, y + 4); g.print('?');
        break;
    }
  }
}
