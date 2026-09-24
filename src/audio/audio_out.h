#pragma once
#include <Arduino.h>
#include "config/config.h"

// I2S output through the ES8311: a small tone synthesizer for alert chimes plus an IMA ADPCM player for the
// spoken clips in the voice pack (/voice.pack in LittleFS, see audio/voice.h).
namespace audio_out {
  struct ClipRef { uint32_t offset = 0, bytes = 0, samples = 0; };   // one entry of the voice pack index

  bool begin(const AudioConfig& ac, uint8_t codecAddr);   // codecAddr 0 = no codec found -> audio disabled
  void apply(const AudioConfig& ac);
  bool available();
  bool inQuietHours();
  bool chime(ChimeStyle style, bool force);   // force ignores quiet hours (test button); returns false when suppressed
  // A chime (ChimeStyle::None = no chime) followed by an optional clip repeated 1..3 times, played as one sequence
  // with the amplifier kept on. Refused while another sound is queued or playing: long sounds are never stacked.
  bool play(ChimeStyle style, const ClipRef* clip, uint8_t repeat, bool force);
  bool busy();                                // something is queued or playing (the voice pack swap waits for this)
  const char* lastSuppressReason();
}
