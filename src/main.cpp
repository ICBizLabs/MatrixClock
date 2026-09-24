// Matrix Weather Clock - Seengreat RGB Matrix HUB75 S3 + 64x32 HUB75 panel.
// Clock + local weather (Open-Meteo) + NWS alerts with a chime, configured through the built-in web UI.
#include <Arduino.h>
#include <LittleFS.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include "pins.h"
#include "version.h"
#include "app.h"
#include "util/log.h"
#include "io/i2c_bus.h"
#include "io/buttons.h"
#include "config/config.h"
#include "display/panel.h"
#include "display/canvas.h"
#include "display/layout.h"
#include "display/renderer.h"
#include "display/frame_snapshot.h"
#include "alarm/alarm.h"
#include "net/lightning.h"
#include "net/pushbullet.h"
#include "net/updater.h"
#include "net/shared_state.h"
#include "net/alert_store.h"
#include "net/wifi_manager.h"
#include "net/net_task.h"
#include "time/time_service.h"
#include "audio/audio_out.h"
#include "web/web_server.h"

static Canvas canvas(layout::W, layout::H);
static uint32_t lastFrame = 0, lastSecond = 0;
static constexpr uint32_t FRAME_MS = 33;
static constexpr uint32_t BOOT_OK_AFTER_MS = 30000;
static constexpr uint32_t MAX_FAILED_BOOTS = 3;
RTC_NOINIT_ATTR static uint32_t g_bootAttempts;   // survives resets (not power cycles): catches boot loops caused by panel settings
static bool bootConfirmed = false;

static void onButton(uint8_t key, bool longPress) {
  LOGI("btn K%u %s", key + 1, longPress ? "long" : "short");
  if (alarmclock::ringing()) {                       // any key deals with a ringing alarm or timer first
    if (key == buttons::K2 || longPress) alarmclock::stop(); else alarmclock::snooze();
    return;
  }
  switch (key) {
    case buttons::K1: if (longPress) renderer::requestTest(10000); else renderer::nextPage(); break;
    case buttons::K2:
      if (longPress) audio_out::chime(g_cfg.audio.chime, true);
      else if (renderer::demoActive()) renderer::setDemo(false);
      else if (renderer::hasMessage()) renderer::clearMessage();
      else alerts::acknowledge("all");
      break;
    case buttons::K3: if (longPress) app::requestReboot(200); else net_task::kick(net_task::JOB_WEATHER | net_task::JOB_ALERTS); break;
  }
}

void setup() {
  // Send every allocation of 4 KB or more to PSRAM (default threshold is 16 KB). Keeps internal RAM free and
  // unfragmented for WiFi, TLS handshakes and task stacks; DMA buffers and stacks ask for internal RAM explicitly.
  heap_caps_malloc_extmem_enable(4096);
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 1500) delay(10);
  log_begin();
  LOGI("MWC %s boot, heap %lu, psram %lu", MWC_VERSION, (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getFreePsram());

  if (!LittleFS.begin(true)) LOGE("LittleFS mount failed");
  config_load();
  app::begin();
  shared::begin();
  alerts::begin();
  pushbullet::begin();
  updater::begin();

  i2c_bus::begin();
  const i2c_bus::Map& i2c = i2c_bus::identify();

  esp_reset_reason_t rr = esp_reset_reason();
  if (rr == ESP_RST_POWERON || rr == ESP_RST_UNKNOWN) g_bootAttempts = 0;   // RTC memory holds garbage after power-on
  g_bootAttempts++;
  if (g_bootAttempts > MAX_FAILED_BOOTS) {
    LOGE("boot loop detected (%lu resets, reason %d): panel settings reset to defaults, brightness lowered",
         (unsigned long)g_bootAttempts, (int)rr);
    g_cfg.panel = PanelConfig();
    if (g_cfg.display.brightness > 64) g_cfg.display.brightness = 64;
    config_save(g_cfg);
    g_bootAttempts = 0;
  }

  if (!panel::begin(g_cfg.panel, g_cfg.display.brightness, g_cfg.display.gamma)) LOGE("HUB75 begin failed");
  else LOGI("HUB75 ok %ux%u @ %d Hz", g_cfg.panel.width, g_cfg.panel.height, panel::refreshRateHz());
  frame_snapshot::begin(layout::W, layout::H);
  renderer::begin(millis());
  if (g_cfg.first_boot) renderer::requestTest(10000);
  alarmclock::begin();

  timesvc::begin(g_cfg.time);
  wifi_mgr::begin(g_cfg.wifi);
  web::begin();
  net_task::start();
  audio_out::begin(g_cfg.audio, i2c.es8311);
  buttons::begin(onButton);
  lightning::begin();
  LOGI("setup done, heap %lu", (unsigned long)ESP.getFreeHeap());
}

void loop() {
  uint32_t now = millis();
  wifi_mgr::loop();
  timesvc::loop();
  buttons::loop();
  app::loop();
  if (!bootConfirmed && now > BOOT_OK_AFTER_MS) { bootConfirmed = true; g_bootAttempts = 0; }
  if (wifi_mgr::consumeConnectedEvent()) {
    renderer::showIp((uint32_t)g_cfg.display.ip_on_connect_sec * 1000UL);
    timesvc::onWifiUp(g_cfg.time);
    net_task::kick(net_task::JOB_WEATHER | net_task::JOB_ALERTS);
  }
  { uint8_t style; if (renderer::consumeDemoSound(style)) audio_out::chime((ChimeStyle)style, true); }
  if (now - lastSecond >= 1000) {
    lastSecond = now;
    alerts::expire(time(nullptr));
    Severity fired;
    if (alerts::takeNewForChime(g_cfg.alerts, g_cfg.audio.repeat_min, now, &fired))
      audio_out::chime(fired == Severity::Extreme ? g_cfg.audio.chime_extreme : g_cfg.audio.chime, false);
    alarmclock::loop(now);
    if (lightning::consumeChimeEvent()) audio_out::chime(g_cfg.audio.chime, false);
    if (lightning::consumeNotifyEvent() && g_cfg.pushbullet.notify_lightning && g_cfg.pushbullet.token[0]) {
      lightning::Status ls = lightning::status();
      char body[96];
      snprintf(body, sizeof(body), "Strike %.1f km %s of home, %u in the last %u min", ls.latest_km, lightning::bearingName(ls.latest_bearing), ls.count, g_cfg.lightning.window_min);
      pushbullet::notify("Lightning nearby", body);
    }
    if (g_cfg.pushbullet.notify_alerts && g_cfg.pushbullet.token[0]) {
      char ev[48], hl[160];
      if (alerts::takeNewForNotify(g_cfg.alerts, g_cfg.pushbullet.notify_min_severity, ev, sizeof(ev), hl, sizeof(hl))) pushbullet::notify(ev, hl);
    }
  }
  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    renderer::tick(canvas, now);
    panel::present(canvas);
    frame_snapshot::update(canvas.getBuffer());
  } else {
    delay(1);
  }
}
