#pragma once
#include <Arduino.h>
#include <time.h>
#include "config/config.h"

// Wall clock: RTC at boot, NTP once WiFi is up, RTC refreshed after every NTP sync.
namespace timesvc {
  enum class Source : uint8_t { None, Rtc, Ntp };
  struct Status { Source source; time_t last_sync; bool rtc_present; };

  void begin(const TimeConfig& tc);
  void onWifiUp(const TimeConfig& tc);        // starts SNTP
  void applyTz(const TimeConfig& tc);         // live timezone / server change
  void loop();                                 // detects NTP syncs and writes the RTC
  bool valid();
  bool localNow(struct tm& lt, uint16_t* ms = nullptr);
  Status status();
  const char* sourceName(Source s);
}
