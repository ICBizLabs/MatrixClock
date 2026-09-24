#include "timeutil.h"
#include <stdio.h>
#include <ctype.h>

int hhmm_parse(const char* s) {
  if (!s) return -1;
  int h = 0, m = 0;
  if (sscanf(s, "%d:%d", &h, &m) != 2) return -1;
  if (h < 0 || h > 23 || m < 0 || m > 59) return -1;
  return h * 60 + m;
}

void hhmm_format(uint16_t minute, char* out, size_t n) {
  snprintf(out, n, "%02u:%02u", (unsigned)(minute / 60) % 24, (unsigned)(minute % 60));
}

bool in_window(uint16_t start, uint16_t end, uint16_t now) {
  if (start == end) return false;
  if (start < end) return now >= start && now < end;
  return now >= start || now < end;   // window crosses midnight
}

int64_t days_from_civil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int64_t)doe - 719468;
}
