#include "app.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "display/renderer.h"
#include "display/panel.h"
#include "audio/audio_out.h"
#include "net/wifi_manager.h"
#include "net/net_task.h"
#include "net/lightning.h"
#include "time/time_service.h"
#include "util/log.h"

namespace app {
  namespace {
    SemaphoreHandle_t cfgMtx = nullptr;
    AppConfig staged;
    uint16_t stagedFlags = 0;
    volatile bool hasStaged = false;
    volatile bool factoryReset = false;
    uint32_t rebootAt = 0;
    uint16_t rebootFlags = 0;
    uint32_t bootMs = 0;

    void applyChanges(uint16_t fl) {
      if (fl & CHG_TIME) timesvc::applyTz(g_cfg.time);
      if (fl & CHG_DISPLAY) { panel::setGamma(g_cfg.display.gamma); renderer::applyDisplay(); }
      if (fl & CHG_AUDIO) audio_out::apply(g_cfg.audio);
      if (fl & (CHG_WEATHER | CHG_LOCATION)) net_task::kick(net_task::JOB_WEATHER);
      if (fl & (CHG_ALERTS | CHG_LOCATION)) net_task::kick(net_task::JOB_ALERTS);
      if (fl & CHG_WIFI) wifi_mgr::applyCredentials(g_cfg.wifi);
      if (fl & CHG_PANEL) { rebootFlags |= CHG_PANEL; panel::setLatchBlanking(g_cfg.panel.latch_blanking); }
      if (fl & (CHG_LIGHTNING | CHG_LOCATION)) lightning::applyConfig();
    }
  }

  void begin() {
    cfgMtx = xSemaphoreCreateRecursiveMutex();
    bootMs = millis();
  }

  void cfgLock() { if (cfgMtx) xSemaphoreTakeRecursive(cfgMtx, pdMS_TO_TICKS(1000)); }
  void cfgUnlock() { if (cfgMtx) xSemaphoreGiveRecursive(cfgMtx); }

  bool stageConfig(const AppConfig& next, uint16_t changed) {
    cfgLock();
    staged = next;
    stagedFlags |= changed;
    hasStaged = true;
    cfgUnlock();
    return true;
  }

  void requestReboot(uint32_t delay_ms) { rebootAt = millis() + delay_ms; if (rebootAt == 0) rebootAt = 1; }
  void requestFactoryReset() { factoryReset = true; }
  bool rebootPending() { return rebootAt != 0; }
  uint32_t uptimeSec() { return (millis() - bootMs) / 1000; }
  uint16_t rebootRequiredFlags() { return rebootFlags; }

  void loop() {
    if (hasStaged) {
      cfgLock();
      AppConfig next = staged;
      uint16_t fl = stagedFlags;
      hasStaged = false;
      stagedFlags = 0;
      g_cfg = next;
      cfgUnlock();
      LOGI("config: applying changes 0x%03X", (unsigned)fl);
      applyChanges(fl);
      config_save(g_cfg);
    }
    if (g_cfg.first_boot && uptimeSec() > 15) {   // the first-boot test pattern has been shown; remember that
      g_cfg.first_boot = false;
      config_save(g_cfg);
    }
    if (factoryReset) { factoryReset = false; config_factory_reset(); }
    if (rebootAt && (int32_t)(millis() - rebootAt) >= 0) {
      LOGW("rebooting");
      delay(100);
      ESP.restart();
    }
  }
}
