#pragma once
#include <stdint.h>

// Negretti & Zambra "Zambretti" barometer forecast: sea-level pressure, its three-hour tendency and the wind direction
// give one of 32 short forecasts (the classic table). Northern hemisphere wind rule: south adds 2, east/west add 1.
namespace zambretti {
  struct Result {
    uint8_t z = 0;             // 1..32 table entry, 0 = not enough data
    int8_t trend = 0;          // -1 falling, 0 steady, 1 rising
    const char* text = "";
  };
  Result forecast(float sea_level_hpa, float delta_3h_hpa, int wind_dir_deg /* -1 = unknown */);
}
