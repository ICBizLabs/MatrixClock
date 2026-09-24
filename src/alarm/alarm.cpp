#include "alarm.h"
#include <time.h>
#include "config/config.h"
#include "audio/audio_out.h"
#include "net/pushbullet.h"
#include "time/time_service.h"
#include "util/log.h"

namespace alarmclock {
  namespace {
    constexpr uint32_t RING_REPEAT_MS = 6000;
    constexpr uint32_t RING_TIMEOUT_MS = 10 * 60000UL;
    constexpr uint32_t SNOOZE_SEC = 9 * 60;

    bool isRinging = false, ringTimer = false;
    ChimeStyle ringStyle = ChimeStyle::TripleBeep;
    char ringLabel[16] = "";
    uint32_t ringSince = 0, lastRing = 0;
    int32_t lastFiredKey[MAX_ALARMS] = { -1, -1, -1, -1 };   // day-of-year * 1440 + minute, so each alarm fires once
    time_t snoozeAt = 0;
    uint32_t timerEndMs = 0;
    bool timerActive = false;

    void startRing(bool timer, ChimeStyle style, const char* label) {
      isRinging = true;
      ringTimer = timer;
      ringStyle = style == ChimeStyle::None ? ChimeStyle::TripleBeep : style;
      strlcpy(ringLabel, label ? label : "", sizeof(ringLabel));
      ringSince = millis();
      lastRing = 0;
      LOGI("alarm: %s ringing%s%s", timer ? "timer" : "alarm", ringLabel[0] ? " " : "", ringLabel);
      if (g_cfg.pushbullet.notify_alarms) pushbullet::notify(timer ? "Timer done" : "Alarm", ringLabel[0] ? ringLabel : (timer ? "The countdown finished" : "Alarm is ringing"));
    }
  }

  void begin() {}

  void loop(uint32_t now_ms) {
    struct tm lt;
    const bool tv = timesvc::localNow(lt);
    if (tv) {
      const int32_t key = lt.tm_yday * 1440 + lt.tm_hour * 60 + lt.tm_min;
      const uint8_t dayBit = (uint8_t)(1 << ((lt.tm_wday + 6) % 7));   // tm_wday: 0 = Sunday -> bit 6
      for (uint8_t i = 0; i < MAX_ALARMS; i++) {
        const AlarmConfig& a = g_cfg.alarms.items[i];
        if (!a.enabled || !(a.days & dayBit) || a.minute != (uint16_t)(lt.tm_hour * 60 + lt.tm_min) || lastFiredKey[i] == key) continue;
        lastFiredKey[i] = key;
        snoozeAt = 0;
        if (!isRinging) startRing(false, a.chime, a.label);
      }
      if (snoozeAt && !isRinging && time(nullptr) >= snoozeAt) { snoozeAt = 0; startRing(false, ringStyle, ringLabel); }
    }
    if (timerActive && (int32_t)(now_ms - timerEndMs) >= 0) {
      timerActive = false;
      startRing(true, g_cfg.audio.chime == ChimeStyle::None ? ChimeStyle::TripleBeep : g_cfg.audio.chime, "TIMER");
    }
    if (isRinging) {
      if (now_ms - ringSince > RING_TIMEOUT_MS) { LOGI("alarm: ring timed out"); isRinging = false; return; }
      if (lastRing == 0 || now_ms - lastRing >= RING_REPEAT_MS) { lastRing = now_ms; audio_out::chime(ringStyle, true); }
    }
  }

  bool ringing() { return isRinging; }
  bool ringingIsTimer() { return ringTimer; }
  const char* ringingLabel() { return ringLabel; }

  void stop() {
    if (isRinging) LOGI("alarm: stopped");
    isRinging = false;
    snoozeAt = 0;
  }

  void snooze() {
    if (!isRinging || ringTimer) { stop(); return; }
    isRinging = false;
    snoozeAt = time(nullptr) + SNOOZE_SEC;
    LOGI("alarm: snoozed for %u min", (unsigned)(SNOOZE_SEC / 60));
  }

  bool snoozed() { return snoozeAt != 0; }
  uint32_t snoozeRemainingSec() { time_t n = time(nullptr); return (snoozeAt && snoozeAt > n) ? (uint32_t)(snoozeAt - n) : 0; }

  bool startTimer(uint32_t seconds) {
    if (seconds == 0 || seconds > 86400) return false;
    timerEndMs = millis() + seconds * 1000UL;
    timerActive = true;
    LOGI("alarm: timer started, %lu s", (unsigned long)seconds);
    return true;
  }

  void cancelTimer() { timerActive = false; if (isRinging && ringTimer) isRinging = false; }
  bool timerRunning() { return timerActive; }
  uint32_t timerRemainingSec() { if (!timerActive) return 0; int32_t d = (int32_t)(timerEndMs - millis()); return d > 0 ? (uint32_t)(d + 999) / 1000 : 0; }
}
