#pragma once
#include <Arduino.h>
#include "config/config.h"

namespace alerts_client {
  String buildUrl(const AppConfig& cfg);
  bool fetch(const AppConfig& cfg, String& err);     // blocking; feeds alerts::applySnapshot on success
  bool parseIso8601(const char* s, time_t& out);     // "2026-09-22T17:00:00-06:00" -> UTC epoch
}
