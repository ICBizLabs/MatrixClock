#pragma once
#include <Arduino.h>
#include "canvas.h"

// Screen state machine: splash, composite clock + bottom-half pages (with slide transitions, effects and holiday
// themes), forecast and hourly full screens, alert banner, messages, alarm/timer, lightning, test pattern, OTA.
namespace renderer {
  void begin(uint32_t now_ms);
  void applyDisplay();                        // after a display config change
  void tick(Canvas& c, uint32_t now_ms);      // draws one frame and keeps panel brightness up to date
  void requestTest(uint32_t hold_ms);
  bool requestFullScreen(const char* name);   // "forecast" or "hourly", shown now for two page periods
  const char* fullScreenBlockReason();        // "" when the periodic full screens can appear, else why not
  void setOta(bool active, uint8_t pct);
  void showIp(uint32_t hold_ms);
  void showMessage(const char* text, uint32_t hold_ms, uint32_t rgb);   // hold_ms 0 = until cleared
  void clearMessage();
  bool hasMessage();
  String messageText();
  uint32_t messageRemainingSec();
  void nextPage();
  void setDemo(bool on, uint32_t total_ms = 10 * 60000UL, bool sound = false);   // cycle demo scenarios (synthetic data), auto-off
  bool demoActive();
  bool demoSound();
  bool consumeDemoSound(uint8_t& style, const char*& phrase);   // true when the demo wants a sound now: chime (ChimeStyle value, None = no chime) and/or a spoken phrase (nullptr = none)
  const char* demoScenario();
  uint32_t demoRemainingSec();
  bool nightActive();
  uint8_t effectiveBrightness();
  const char* screenName();
  const char* themeName();                    // active holiday theme or ""
}
