#pragma once
#include <Arduino.h>
#include "config/config.h"

// Infrared remote receiver (VS1838B / TSOP38238 style module on one GPIO, default 44 = the RX0 pad). The RMT
// peripheral captures the pulse train; NEC frames are decoded to their 32-bit code, anything else gets a stable
// hash of its timing so any remote can be learned. Codes map to actions through the remote config.
namespace ir_remote {
  struct Status {
    bool enabled;
    int8_t pin;
    uint32_t last_code;      // 0 = nothing received yet
    char last_proto[8];      // "nec", "nec-rpt", "hash"
    uint32_t last_ms;        // millis() of the last code, 0 = never
    bool learning;
    uint32_t received;       // frames decoded since boot
  };
  void begin(const RemoteConfig& cfg);
  void apply(const RemoteConfig& cfg);
  void loop();               // main loop: dispatches actions
  void startLearn();         // the next code is only recorded, not executed
  Status status();
}
