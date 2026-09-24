#pragma once
#include <Arduino.h>

// Alarm clock (up to MAX_ALARMS entries in the config) and a single countdown timer. Both "ring" by repeating the
// configured chime and taking over the bottom half of the display until stopped, snoozed or timed out.
namespace alarmclock {
  void begin();
  void loop(uint32_t now_ms);                 // call once per second
  bool ringing();
  bool ringingIsTimer();
  const char* ringingLabel();
  void stop();                                // stop the current ring (alarm or timer)
  void snooze();                              // alarms only: ring again in 9 minutes
  bool snoozed();
  uint32_t snoozeRemainingSec();
  bool startTimer(uint32_t seconds);          // 1 s .. 24 h
  void cancelTimer();
  bool timerRunning();
  uint32_t timerRemainingSec();
}
