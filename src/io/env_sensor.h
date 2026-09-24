#pragma once
#include <Arduino.h>
#include "config/config.h"

// Indoor environment sensor on the I2C header: Bosch BME280 (temperature, humidity, pressure), BMP280 (no humidity)
// or BME680/688 (plus a MOX gas sensor for volatile organic compounds). Auto-detected at 0x76 / 0x77 by chip ID.
// Sampled from the main loop in forced mode; one history point per minute for 24 hours lives in PSRAM and feeds the
// rising / falling indicators. The gas reading becomes a relative 0..100 air-quality score against a learned
// clean-air baseline (kept in LittleFS across reboots).
namespace env_sensor {
  enum class Type : uint8_t { None = 0, BMP280, BME280, BME680 };
  enum class Trend : int8_t { FallingFast = -2, Falling = -1, Steady = 0, Rising = 1, RisingFast = 2 };
  enum AirLevel : uint8_t { AIR_UNKNOWN = 0, AIR_GOOD = 1, AIR_FAIR = 2, AIR_POOR = 3 };
  struct Reading {
    bool valid = false;
    bool has_humidity = false;
    float temp_c = 0;            // after the calibration offset
    float humidity = 0;          // % RH after the calibration offset
    float pressure_hpa = 0;      // station pressure
    float sea_level_hpa = 0;     // reduced to sea level when an altitude is known, else == pressure_hpa
    bool sea_level_known = false;
    float altitude_m = 0;
    uint32_t sample_ms = 0;
    Trend t_temp = Trend::Steady, t_hum = Trend::Steady, t_press = Trend::Steady, t_air = Trend::Steady;
    float d_temp = 0, d_hum = 0, d_press = 0, d_air = 0;   // change over the trend windows (pressure scaled to the full window)
    uint16_t span_min = 0;       // minutes of history behind the trends (0 = not enough history yet)
    // BME680 gas sensor
    bool has_gas = false;        // BME680 with the heater enabled
    bool gas_valid = false;      // heater stable, measurement valid
    float gas_kohm = 0;          // gas resistance
    float air_score = 0;         // 0..100, 100 = clean (75 % gas resistance vs baseline, 25 % humidity comfort)
    float air_baseline_kohm = 0; // learned clean-air resistance
    bool air_ready = false;      // burn-in done and a baseline exists
    uint8_t air_level = AIR_UNKNOWN;
    // derived comfort values
    float dew_point_c = 0;
    float abs_humidity = 0;      // g/m3
    float heat_index_c = 0;      // == temp_c when it does not apply
    uint8_t condensation = 0;    // 0 none / unknown, 1 possible, 2 likely (indoor dew point vs outdoor temperature)
    uint8_t mould_risk = 0;      // 0 none, 1 elevated (>= 60 % RH for 6 h), 2 high (>= 70 % RH for 2 h)
  };
  struct HistoryPoint { uint16_t age_min; float temp_c, humidity, pressure_hpa, gas_kohm, air_score; };

  void begin(const IndoorConfig& cfg);          // probes the bus; safe to call without a sensor
  void apply(const IndoorConfig& cfg);          // CHG_INDOOR
  void loop(uint32_t now_ms);                   // main loop: non-blocking sampling
  void setAltitudeHint(float meters);           // elevation from the weather service, used when indoor.altitude_m < 0
  void setOutdoorTempC(float c);                // current outdoor temperature (condensation risk); NAN = unknown
  Type type();
  const char* typeName();
  uint8_t address();
  bool present();
  bool hasGas();
  Reading reading();
  size_t history(HistoryPoint* out, size_t max, uint16_t minutes, uint16_t step_min);   // oldest first
  bool consumeAirAlert();                       // true once when the air has been poor long enough to tell someone
  const char* trendName(Trend t);
  const char* airLevelName(uint8_t level);
  const char* condensationName(uint8_t c);
  const char* mouldName(uint8_t m);
  uint32_t errors();
}
