#pragma once
#include <Arduino.h>
#include "config/config.h"

// Application-level glue: config staging from web handlers, reboot/reset requests, config mutex.
namespace app {
  void begin();
  void loop();                                              // applies staged config, executes reboot / factory reset
  void cfgLock();
  void cfgUnlock();
  bool stageConfig(const AppConfig& next, uint16_t changed); // from async web handlers; applied on the main loop
  void requestReboot(uint32_t delay_ms);
  void requestFactoryReset();
  bool rebootPending();
  uint32_t uptimeSec();
  uint16_t rebootRequiredFlags();                            // CHG_* bits that need a reboot to take effect
}
