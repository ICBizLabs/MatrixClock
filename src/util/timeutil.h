#pragma once
#include <stdint.h>
#include <stddef.h>

// Minute-of-day helpers used by brightness schedules, night mode and quiet hours.
int  hhmm_parse(const char* s);                               // "HH:MM" -> 0..1439, or -1 on error
void hhmm_format(uint16_t minute, char* out, size_t n);       // 0..1439 -> "HH:MM"
bool in_window(uint16_t start, uint16_t end, uint16_t now);   // true when now is inside [start, end); wraps past midnight

// Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's algorithm); no timezone involved.
int64_t days_from_civil(int y, unsigned m, unsigned d);
