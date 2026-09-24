#include "zambretti.h"
#include <math.h>

namespace zambretti {
  namespace {
    const char* const TEXT[33] = { "",
      "Settled fine", "Fine weather", "Fine, becoming less settled", "Fairly fine, showery later", "Showery, becoming more unsettled",
      "Unsettled, rain later", "Rain at times, worse later", "Rain at times, becoming very unsettled", "Very unsettled, rain",
      "Settled fine", "Fine weather", "Fine, possibly showers", "Fairly fine, showers likely", "Showery, bright intervals",
      "Changeable, some rain", "Unsettled, rain at times", "Rain at frequent intervals", "Very unsettled, rain", "Stormy, much rain",
      "Settled fine", "Fine weather", "Becoming fine", "Fairly fine, improving", "Fairly fine, possibly showers early",
      "Showery early, improving", "Changeable, mending", "Rather unsettled, clearing later", "Unsettled, probably improving",
      "Unsettled, short fine intervals", "Very unsettled, finer at times", "Stormy, possibly improving", "Stormy, much rain" };
  }

  Result forecast(float p, float d3h, int wind) {
    Result r;
    if (isnan(p) || p < 900 || p > 1100) return r;
    int lo, hi;
    float z;
    if (d3h <= -1.6f) { r.trend = -1; z = 127.0f - 0.12f * p; lo = 1; hi = 9; }
    else if (d3h >= 1.6f) { r.trend = 1; z = 185.0f - 0.16f * p; lo = 20; hi = 32; }
    else { r.trend = 0; z = 144.0f - 0.13f * p; lo = 10; hi = 19; }
    int zi = (int)lroundf(z);
    if (wind >= 0) {
      int w = ((wind % 360) + 360) % 360;
      if (w >= 135 && w <= 225) zi += 2;              // southerly: worse
      else if (!(w >= 315 || w <= 45)) zi += 1;       // east or west
    }
    if (zi < lo) zi = lo;
    if (zi > hi) zi = hi;
    r.z = (uint8_t)zi;
    r.text = TEXT[zi];
    return r;
  }
}
