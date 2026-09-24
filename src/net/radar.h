#pragma once
#include <Arduino.h>
#include "config/config.h"

// Animated weather radar: NEXRAD base reflectivity composites from the Iowa Environmental Mesonet WMS
// (https://mesonet.agron.iastate.edu/ogc/), cropped and scaled by the server to the panel size around the
// configured location. The newest composite plus the 5..50 minutes-ago layers give an eleven-frame loop; after the
// first fill only the newest frame is fetched every few minutes. Frames are RGB565 in PSRAM. Runs in the network task.
namespace radar {
  constexpr uint8_t MAX_FRAMES = 12;
  constexpr uint16_t W = 64, H = 32;
  struct Status {
    bool enabled;
    uint8_t frames;
    uint32_t last_ok_ms;      // 0 = never
    uint32_t last_err_ms;
    uint8_t fails;
    char err[48];
    bool echo_near;           // echoes within the central third of the newest frame
    uint8_t echo_pct;         // percent of pixels with echoes in the newest frame
  };
  void begin();
  void applyConfig();                            // CHG_RADAR / CHG_LOCATION: forget the frames, refetch
  void requestRefresh();
  bool due(const AppConfig& cfg, uint32_t now_ms);
  void run(const AppConfig& cfg);                // network task
  Status status();
  uint8_t frameCount();
  int32_t frameAgeMin(uint8_t i);                // i = 0 oldest .. frameCount()-1 newest; -1 when missing
  bool copyFrame(uint8_t i, uint16_t* out);      // W*H pixels
  size_t copyFrameBytes(uint8_t i, uint8_t* out, size_t offset, size_t maxLen);   // for the web preview
  bool echoNearby();
}
