#pragma once
#include <Arduino.h>
#include "config/config.h"

// I2S output through the ES8311 with a small tone synthesizer for alert chimes.
namespace audio_out {
  bool begin(const AudioConfig& ac, uint8_t codecAddr);   // codecAddr 0 = no codec found -> audio disabled
  void apply(const AudioConfig& ac);
  bool available();
  bool inQuietHours();
  bool chime(ChimeStyle style, bool force);   // force ignores quiet hours (test button); returns false when suppressed
  const char* lastSuppressReason();
}
