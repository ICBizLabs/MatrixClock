#pragma once
#include <Arduino.h>
#include "config/config.h"

// Indoor environment sensor on the I2C header: Bosch BME280 (temperature, humidity, pressure), BMP280 (no humidity)
// or BME680/688 (gas sensor left off). Auto-detected at 0x76 / 0x77 by chip ID. Sampled from the main loop in forced
// mode (no self-heating between samples); one history point per minute for 24 hours lives in PSRAM and feeds the
// rising / falling indicators.
namespace env_sensor {
  enum class Type : uint8_t { None = 0, BMP280, BME280, BME680 };
  enum class Trend : int8_t { FallingFast = -2, Falling = -1, Steady = 0, Rising = 1, RisingFast = 2 };
  struct Reading {
    bool valid = false;
    bool has_humidity = false;
    float temp_c = 0;            // after the calibration offset
    float humidity = 0;          // % RH after the calibration offset
    float pressure_hpa = 0;      // station pressure
    float sea_level_hpa = 0;     // reduced to sea level when an altitude is known, else == pressure_hpa
    bool sea_level_known = false;
    float altitude_m = 0;        // altitude used for the reduction
    uint32_t sample_ms = 0;
    Trend t_temp = Trend::Steady, t_hum = Trend::Steady, t_press = Trend::Steady;
    float d_temp = 0, d_hum = 0, d_press = 0;   // change over the trend windows (C, %, hPa; pressure scaled to the full window)
    uint16_t span_min = 0;       // minutes of history behind the trends (0 = not enough history yet)
  };
  struct HistoryPoint { uint16_t age_min; float temp_c, humidity, pressure_hpa; };

  void begin(const IndoorConfig& cfg);          // probes the bus; safe to call without a sensor
  void apply(const IndoorConfig& cfg);          // CHG_INDOOR
  void loop(uint32_t now_ms);                   // main loop: non-blocking sampling
  void setAltitudeHint(float meters);           // elevation from the weather service, used when indoor.altitude_m < 0
  Type type();
  const char* typeName();
  uint8_t address();
  bool present();
  Reading reading();
  size_t history(HistoryPoint* out, size_t max, uint16_t minutes, uint16_t step_min);   // oldest first
  const char* trendName(Trend t);
  uint32_t errors();
}
