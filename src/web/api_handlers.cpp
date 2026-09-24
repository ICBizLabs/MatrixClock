// REST API: /api/* endpoints. Handlers run on the async_tcp task: they only copy state under mutexes and stage
// changes for the main loop; they never touch the network, I2C or the renderer directly.
#include "web_server.h"
#include <AsyncJson.h>
#include <esp_heap_caps.h>
#include "app.h"
#include "version.h"
#include "config/config.h"
#include "display/panel.h"
#include "display/renderer.h"
#include "display/wmo.h"
#include "net/wifi_manager.h"
#include "net/net_task.h"
#include "net/shared_state.h"
#include "net/alert_store.h"
#include "audio/audio_out.h"
#include "io/i2c_bus.h"
#include "io/buttons.h"
#include "time/time_service.h"
#include "time/tz_table.h"
#include "alarm/alarm.h"
#include "net/lightning.h"
#include "net/pushbullet.h"
#include "display/frame_snapshot.h"
#include "util/log.h"

namespace web {
  namespace {
    uint32_t ageSec(uint32_t stamp_ms) { return stamp_ms ? (millis() - stamp_ms) / 1000 : 0; }

    void weatherToJson(JsonObject o, const WeatherData& w) {
      o["valid"] = w.valid;
      if (!w.valid) return;
      o["units"] = w.imperial ? "imperial" : "metric";
      o["age_s"] = ageSec(w.fetched_ms);
      o["utc_offset_s"] = w.utc_offset_s;
      JsonObject c = o["cur"].to<JsonObject>();
      c["temp"] = w.cur.temp; c["feels"] = w.cur.feels; c["humidity"] = w.cur.humidity;
      c["wind"] = w.cur.wind; c["gust"] = w.cur.gust; c["wind_dir"] = w.cur.wind_dir;
      c["wmo"] = w.cur.wmo; c["text"] = wmo::text(w.cur.wmo); c["is_day"] = w.cur.is_day;
      JsonArray d = o["daily"].to<JsonArray>();
      for (uint8_t i = 0; i < w.ndaily; i++) {
        JsonObject x = d.add<JsonObject>();
        x["date"] = w.daily[i].date; x["wmo"] = w.daily[i].wmo; x["text"] = wmo::text(w.daily[i].wmo);
        x["tmax"] = w.daily[i].tmax; x["tmin"] = w.daily[i].tmin; x["pop"] = w.daily[i].pop;
      }
    }

    void handleStatus(AsyncWebServerRequest* r) {
      AsyncJsonResponse* res = new AsyncJsonResponse();
      JsonObject root = res->getRoot().to<JsonObject>();
      root["version"] = MWC_VERSION;
      root["uptime_s"] = app::uptimeSec();

      JsonObject t = root["time"].to<JsonObject>();
      struct tm lt; uint16_t ms;
      bool tv = timesvc::localNow(lt, &ms);
      t["valid"] = tv;
      if (tv) { char b[32]; strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", &lt); t["local"] = b; }
      timesvc::Status ts = timesvc::status();
      t["source"] = timesvc::sourceName(ts.source);
      t["last_sync"] = (long)ts.last_sync;
      t["rtc_present"] = ts.rtc_present;
      app::cfgLock();
      t["tz"] = g_cfg.time.tz_posix;
      AlertsConfig ac = g_cfg.alerts;
      app::cfgUnlock();

      WeatherData w;
      shared::getWeather(w);
      weatherToJson(root["weather"].to<JsonObject>(), w);

      JsonObject al = root["alerts"].to<JsonObject>();
      al["count"] = alerts::count();
      al["stale"] = alerts::stale();
      alerts::toJson(al["items"].to<JsonArray>(), ac, false);

      JsonObject wi = root["wifi"].to<JsonObject>();
      wi["connected"] = wifi_mgr::isConnected();
      wi["ap"] = wifi_mgr::apActive();
      wi["ap_ssid"] = wifi_mgr::apSsid();
      wi["ip"] = wifi_mgr::ip().toString();
      wi["rssi"] = wifi_mgr::rssi();

      NetStatus ns;
      shared::getNet(ns);
      JsonObject n = root["net"].to<JsonObject>();
      n["wx_ok_age_s"] = ageSec(ns.last_wx_ok); n["wx_err_age_s"] = ageSec(ns.last_wx_err); n["wx_err"] = ns.wx_err; n["wx_fails"] = ns.wx_fails;
      n["al_ok_age_s"] = ageSec(ns.last_al_ok); n["al_err_age_s"] = ageSec(ns.last_al_err); n["al_err"] = ns.al_err; n["al_fails"] = ns.al_fails;

      JsonObject sys = root["sys"].to<JsonObject>();
      sys["heap_free"] = ESP.getFreeHeap();
      sys["heap_largest"] = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      sys["psram_free"] = ESP.getFreePsram();
      sys["refresh_hz"] = panel::refreshRateHz();
      sys["panel_ok"] = panel::valid();
      sys["driver"] = panel::driverName();
      sys["screen"] = renderer::screenName();
      sys["brightness"] = renderer::effectiveBrightness();
      sys["night"] = renderer::nightActive();
      sys["reboot_required"] = app::rebootRequiredFlags() != 0;
      JsonObject am = root["alarm"].to<JsonObject>();
      am["ringing"] = alarmclock::ringing();
      am["ringing_timer"] = alarmclock::ringingIsTimer();
      am["label"] = alarmclock::ringingLabel();
      am["snoozed"] = alarmclock::snoozed();
      am["snooze_remaining_s"] = alarmclock::snoozeRemainingSec();
      am["timer_running"] = alarmclock::timerRunning();
      am["timer_remaining_s"] = alarmclock::timerRemainingSec();
      JsonObject mg = root["message"].to<JsonObject>();
      mg["active"] = renderer::hasMessage();
      mg["text"] = renderer::messageText();
      mg["remaining_s"] = renderer::messageRemainingSec();
      lightning::Status lst = lightning::status();
      JsonObject lg = root["lightning"].to<JsonObject>();
      lg["enabled"] = lst.enabled; lg["connected"] = lst.connected; lg["active"] = lst.active;
      lg["count"] = lst.count; lg["nearest_km"] = lst.nearest_km; lg["latest_km"] = lst.latest_km;
      lg["latest_bearing"] = lst.latest_bearing; lg["latest_age_s"] = lst.latest_time ? (long)(time(nullptr) - lst.latest_time) : -1;
      lg["total"] = lst.total;
      sys["theme"] = renderer::themeName();
      sys["full_screen_block"] = renderer::fullScreenBlockReason();
      pushbullet::Status pbs = pushbullet::status();
      JsonObject pb = root["pushbullet"].to<JsonObject>();
      pb["configured"] = pbs.configured; pb["device_ok"] = pbs.device_ok; pb["sent"] = pbs.sent; pb["received"] = pbs.received;
      pb["errors"] = pbs.errors; pb["last_err"] = pbs.last_err; pb["last_poll_age_s"] = pbs.last_poll_ms ? (millis() - pbs.last_poll_ms) / 1000 : 0;
      JsonObject au = root["audio"].to<JsonObject>();
      au["available"] = audio_out::available();
      au["quiet_now"] = audio_out::inQuietHours();
      au["buttons"] = buttons::available();
      const i2c_bus::Map& m = i2c_bus::map();
      JsonObject i2c = root["i2c"].to<JsonObject>();
      i2c["es8311"] = m.es8311; i2c["pca9557"] = m.pca9557; i2c["rtc"] = m.rtc;
      res->setLength();
      r->send(res);
    }

    void handleConfigGet(AsyncWebServerRequest* r) {
      AsyncJsonResponse* res = new AsyncJsonResponse();
      JsonObject root = res->getRoot().to<JsonObject>();
      const bool download = r->hasParam("download");     // backup: unmasked secrets, served as a file
      app::cfgLock();
      config_to_json(g_cfg, root, !download);
      app::cfgUnlock();
      if (download) {
        res->addHeader("Content-Disposition", "attachment; filename=\"matrix-clock-config.json\"");
        res->setLength();
        r->send(res);
        return;
      }
      JsonArray tz = root["tz_options"].to<JsonArray>();
      for (size_t i = 0; i < TZ_TABLE_LEN; i++) {
        JsonObject o = tz.add<JsonObject>();
        o["id"] = TZ_TABLE[i].id; o["label"] = TZ_TABLE[i].label; o["posix"] = TZ_TABLE[i].posix;
      }
      res->setLength();
      r->send(res);
    }

    void handleConfigPost(AsyncWebServerRequest* r, JsonVariant& json) {
      app::cfgLock();
      AppConfig next = g_cfg;
      app::cfgUnlock();
      uint16_t changed = 0;
      String err;
      if (!config_from_json(json.as<JsonObjectConst>(), next, changed, err)) { sendJsonError(r, 400, err); return; }
      // timezone id from the table fills tz_posix unless a custom string was sent
      JsonObjectConst tj = json.as<JsonObjectConst>()["time"];
      if (!tj.isNull() && tj["tz_id"].is<const char*>() && !tj["tz_posix"].is<const char*>()) {
        const char* posix = tz_posix_for(tj["tz_id"].as<const char*>());
        if (posix) strlcpy(next.time.tz_posix, posix, sizeof(next.time.tz_posix));
      }
      if ((changed & CHG_ALERTS) && next.alerts.enabled && next.alerts.user_agent_contact[0] == '\0') { sendJsonError(r, 400, "alerts.user_agent_contact: NWS requires a contact (e-mail or URL)"); return; }
      app::stageConfig(next, changed);
      AsyncJsonResponse* res = new AsyncJsonResponse();
      JsonObject root = res->getRoot().to<JsonObject>();
      root["ok"] = true;
      JsonArray applied = root["applied"].to<JsonArray>();
      JsonArray reboot = root["reboot_required"].to<JsonArray>();
      struct { uint16_t bit; const char* name; bool reboot; } sections[] = {
        { CHG_WIFI, "wifi", false }, { CHG_LOCATION, "location", false }, { CHG_TIME, "time", false }, { CHG_WEATHER, "weather", false },
        { CHG_ALERTS, "alerts", false }, { CHG_DISPLAY, "display", false }, { CHG_PANEL, "panel", true }, { CHG_AUDIO, "audio", false },
        { CHG_ALARMS, "alarms", false }, { CHG_LIGHTNING, "lightning", false }, { CHG_PUSHBULLET, "pushbullet", false } };
      for (auto& s : sections) if (changed & s.bit) (s.reboot ? reboot : applied).add(s.name);
      res->setLength();
      r->send(res);
    }

    void handleWeather(AsyncWebServerRequest* r) {
      AsyncJsonResponse* res = new AsyncJsonResponse();
      WeatherData w;
      shared::getWeather(w);
      weatherToJson(res->getRoot().to<JsonObject>(), w);
      res->setLength();
      r->send(res);
    }

    void handleAlerts(AsyncWebServerRequest* r) {
      AsyncJsonResponse* res = new AsyncJsonResponse();
      JsonObject root = res->getRoot().to<JsonObject>();
      app::cfgLock();
      AlertsConfig ac = g_cfg.alerts;
      app::cfgUnlock();
      root["stale"] = alerts::stale();
      alerts::toJson(root["items"].to<JsonArray>(), ac, r->hasParam("all"));
      res->setLength();
      r->send(res);
    }

    void handleTestAlert(AsyncWebServerRequest* r, JsonVariant& json) {
      JsonObjectConst o = json.as<JsonObjectConst>();
      Severity s = Severity::Severe;
      if (o["severity"].is<const char*>() && !severity_parse(o["severity"], s)) { sendJsonError(r, 400, "unknown severity"); return; }
      uint16_t minutes = o["minutes"] | 3;
      alerts::injectTest(o["event"] | "Test Alert", s, o["headline"] | "This is a test alert from the matrix weather clock", minutes, millis());
      r->send(200, "application/json", "{\"ok\":true}");
    }

    void handleTestChime(AsyncWebServerRequest* r, JsonVariant& json) {
      JsonObjectConst o = json.as<JsonObjectConst>();
      app::cfgLock();
      ChimeStyle style = g_cfg.audio.chime;
      app::cfgUnlock();
      if (o["style"].is<const char*>() && !chime_parse(o["style"], style)) { sendJsonError(r, 400, "unknown chime style"); return; }
      bool force = o["force"] | true;
      if (audio_out::chime(style == ChimeStyle::None ? ChimeStyle::TwoTone : style, force)) r->send(200, "application/json", "{\"ok\":true}");
      else sendJsonError(r, 409, String("chime suppressed: ") + audio_out::lastSuppressReason());
    }

    void handleMessage(AsyncWebServerRequest* r, JsonVariant& json) {
      JsonObjectConst o = json.as<JsonObjectConst>();
      const char* text = o["text"] | "";
      if (!*text) { sendJsonError(r, 400, "text required"); return; }
      if (strlen(text) > 200) { sendJsonError(r, 400, "text too long (max 200)"); return; }
      long sec = o["seconds"] | 30;
      if (sec < 0 || sec > 86400) { sendJsonError(r, 400, "seconds out of range"); return; }
      uint32_t rgb = 0xFFFFFF;
      const char* col = o["color"] | "";
      if (col[0] == '#' && strlen(col) == 7) rgb = strtoul(col + 1, nullptr, 16);
      app::cfgLock();
      ChimeStyle style = g_cfg.audio.chime;
      app::cfgUnlock();
      renderer::showMessage(text, (uint32_t)sec * 1000UL, rgb);
      if (o["chime"] | false) audio_out::chime(style, o["force"] | false);
      r->send(200, "application/json", "{\"ok\":true}");
    }

    void handleTimer(AsyncWebServerRequest* r, JsonVariant& json) {
      long sec = json.as<JsonObjectConst>()["seconds"] | 0;
      if (sec <= 0) { long m = json.as<JsonObjectConst>()["minutes"] | 0; sec = m * 60; }
      if (!alarmclock::startTimer((uint32_t)max(0L, sec))) { sendJsonError(r, 400, "seconds must be 1..86400"); return; }
      r->send(200, "application/json", "{\"ok\":true}");
    }

    void handleFrame(AsyncWebServerRequest* r) {
      AsyncWebServerResponse* res = r->beginChunkedResponse("application/octet-stream",
        [](uint8_t* buf, size_t maxLen, size_t index) -> size_t { return frame_snapshot::copy(buf, index, maxLen); });
      res->addHeader("Cache-Control", "no-store");
      char dims[24];
      snprintf(dims, sizeof(dims), "%ux%u", frame_snapshot::width(), frame_snapshot::height());
      res->addHeader("X-Frame-Size", dims);
      r->send(res);
    }

    void handleWifiScan(AsyncWebServerRequest* r) {
      AsyncJsonResponse* res = new AsyncJsonResponse();
      JsonObject root = res->getRoot().to<JsonObject>();
      if (r->hasParam("start")) { wifi_mgr::startScan(); root["scanning"] = true; }
      else {
        int n = wifi_mgr::scanToJson(root["networks"].to<JsonArray>());
        root["scanning"] = n < 0;
        if (n == 0 && !r->hasParam("poll")) wifi_mgr::startScan();
      }
      res->setLength();
      r->send(res);
    }
  }

  void registerApi(AsyncWebServer& server) {
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/config", HTTP_GET, handleConfigGet);
    server.on("/api/weather", HTTP_GET, handleWeather);
    server.on("/api/alerts", HTTP_GET, handleAlerts);
    server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
    server.on("/api/log", HTTP_GET, [](AsyncWebServerRequest* r) { r->send(200, "text/plain", log_dump()); });
    server.on("/api/test/panel", HTTP_POST, [](AsyncWebServerRequest* r) {
      long sec = 10;
      if (r->hasParam("sec", true)) sec = r->getParam("sec", true)->value().toInt();
      else if (r->hasParam("sec")) sec = r->getParam("sec")->value().toInt();
      renderer::requestTest((uint32_t)constrain(sec, 3, 300) * 1000UL);
      r->send(200, "application/json", "{\"ok\":true}");
    });
    server.on("/api/alerts/ack", HTTP_POST, [](AsyncWebServerRequest* r) {
      String id = r->hasParam("id", true) ? r->getParam("id", true)->value() : (r->hasParam("id") ? r->getParam("id")->value() : "all");
      alerts::acknowledge(id.c_str());
      r->send(200, "application/json", "{\"ok\":true}");
    });
    server.on("/api/frame", HTTP_GET, handleFrame);
    server.on("/api/show", HTTP_POST, [](AsyncWebServerRequest* r) {
      String which = r->hasParam("screen", true) ? r->getParam("screen", true)->value() : (r->hasParam("screen") ? r->getParam("screen")->value() : "forecast");
      if (renderer::requestFullScreen(which.c_str())) r->send(200, "application/json", "{\"ok\":true}");
      else sendJsonError(r, 409, "screen not available (no weather data yet?)");
    });
    server.on("/api/test/push", HTTP_POST, [](AsyncWebServerRequest* r) {
      if (pushbullet::notify("Matrix Clock", "Test notification from your clock")) r->send(200, "application/json", "{\"ok\":true}");
      else sendJsonError(r, 409, "Pushbullet token not set");
    });
    server.on("/api/message/clear", HTTP_POST, [](AsyncWebServerRequest* r) { renderer::clearMessage(); r->send(200, "application/json", "{\"ok\":true}"); });
    server.on("/api/alarm/stop", HTTP_POST, [](AsyncWebServerRequest* r) { alarmclock::stop(); r->send(200, "application/json", "{\"ok\":true}"); });
    server.on("/api/alarm/snooze", HTTP_POST, [](AsyncWebServerRequest* r) { alarmclock::snooze(); r->send(200, "application/json", "{\"ok\":true}"); });
    server.on("/api/timer/cancel", HTTP_POST, [](AsyncWebServerRequest* r) { alarmclock::cancelTimer(); r->send(200, "application/json", "{\"ok\":true}"); });
    server.on("/api/refresh", HTTP_POST, [](AsyncWebServerRequest* r) { net_task::kick(net_task::JOB_WEATHER | net_task::JOB_ALERTS); r->send(200, "application/json", "{\"ok\":true}"); });
    server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* r) { r->send(200, "application/json", "{\"ok\":true}"); app::requestReboot(500); });
    server.on("/api/factory-reset", HTTP_POST, [](AsyncWebServerRequest* r) {
      bool confirmed = (r->hasParam("confirm", true) && r->getParam("confirm", true)->value() == "yes") || (r->hasParam("confirm") && r->getParam("confirm")->value() == "yes");
      if (!confirmed) { sendJsonError(r, 400, "send confirm=yes"); return; }
      r->send(200, "application/json", "{\"ok\":true}");
      app::requestFactoryReset();
    });

    auto* cfgPost = new AsyncCallbackJsonWebHandler("/api/config", handleConfigPost);
    cfgPost->setMethod(HTTP_POST);
    cfgPost->setMaxContentLength(8192);
    server.addHandler(cfgPost);
    auto* testAlert = new AsyncCallbackJsonWebHandler("/api/test/alert", handleTestAlert);
    testAlert->setMethod(HTTP_POST);
    server.addHandler(testAlert);
    auto* testChime = new AsyncCallbackJsonWebHandler("/api/test/chime", handleTestChime);
    testChime->setMethod(HTTP_POST);
    server.addHandler(testChime);
    auto* message = new AsyncCallbackJsonWebHandler("/api/message", handleMessage);
    message->setMethod(HTTP_POST);
    server.addHandler(message);
    auto* timer = new AsyncCallbackJsonWebHandler("/api/timer", handleTimer);
    timer->setMethod(HTTP_POST);
    server.addHandler(timer);
  }
}
