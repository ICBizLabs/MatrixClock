#pragma once
#include <string.h>

// US time zones offered in the web UI; tz_posix is what the firmware actually applies (setenv TZ).
struct TzEntry { const char* id; const char* label; const char* posix; };
static const TzEntry TZ_TABLE[] = {
  { "America/New_York",    "Eastern",       "EST5EDT,M3.2.0,M11.1.0" },
  { "America/Chicago",     "Central",       "CST6CDT,M3.2.0,M11.1.0" },
  { "America/Denver",      "Mountain",      "MST7MDT,M3.2.0,M11.1.0" },
  { "America/Phoenix",     "Arizona",       "MST7" },
  { "America/Los_Angeles", "Pacific",       "PST8PDT,M3.2.0,M11.1.0" },
  { "America/Anchorage",   "Alaska",        "AKST9AKDT,M3.2.0,M11.1.0" },
  { "Pacific/Honolulu",    "Hawaii",        "HST10" },
  { "America/Puerto_Rico", "Atlantic (PR)", "AST4" },
  { "Pacific/Guam",        "Guam",          "ChST-10" },
  { "Etc/UTC",             "UTC",           "UTC0" },
};
constexpr size_t TZ_TABLE_LEN = sizeof(TZ_TABLE) / sizeof(TZ_TABLE[0]);

inline const char* tz_posix_for(const char* id) {
  for (size_t i = 0; i < TZ_TABLE_LEN; i++) if (strcmp(TZ_TABLE[i].id, id) == 0) return TZ_TABLE[i].posix;
  return nullptr;
}
