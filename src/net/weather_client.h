#pragma once
#include <Arduino.h>
#include "config/config.h"
#include "shared_state.h"

namespace weather_client {
  String buildUrl(const AppConfig& cfg);
  bool fetch(const AppConfig& cfg, WeatherData& out, String& err);   // blocking, network task only
}
