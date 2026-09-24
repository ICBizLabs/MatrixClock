#include "pushbullet.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <time.h>
#include "http_util.h"
#include "net_task.h"
#include "app.h"
#include "audio/audio_out.h"
#include "display/renderer.h"
#include "util/psram_alloc.h"
#include "util/log.h"
#include "version.h"

namespace pushbullet {
  namespace {
    constexpr const char* API = "https://api.pushbullet.com/v2/";
    struct Item { char title[48]; char body[160]; };
    constexpr uint8_t QUEUE_LEN = 4;
    Item queue[QUEUE_LEN];
    uint8_t qhead = 0, qcount = 0;
    SemaphoreHandle_t mtx = nullptr;
    Status st = {};
    double modifiedAfter = 0;          // Pushbullet "modified" timestamps are fractional seconds
    String ownIdens[8]; uint8_t ownIdx = 0;
    uint32_t nextPoll = 0;
    bool deviceChecked = false;

    bool take() { return mtx && xSemaphoreTake(mtx, pdMS_TO_TICKS(100)) == pdTRUE; }
    void give() { xSemaphoreGive(mtx); }
    void setErr(const String& e) { strlcpy(st.last_err, e.c_str(), sizeof(st.last_err)); st.errors++; }

    String jsonEscape(const char* s) {
      String o;
      for (; *s; s++) {
        if (*s == '"' || *s == '\\') { o += '\\'; o += *s; }
        else if (*s == '\n') o += "\\n";
        else if ((uint8_t)*s < 0x20) continue;
        else o += *s;
      }
      return o;
    }

    // Registers the clock as a Pushbullet device once, so pushes can be targeted at it and sends show its name.
    bool ensureDevice(const AppConfig& cfg) {
      if (cfg.pushbullet.device_iden[0]) { st.device_ok = true; return true; }
      http_util::Options opt;
      opt.userAgent = MWC_USER_AGENT_NAME "/" MWC_VERSION;
      opt.headerName = "Access-Token"; opt.headerValue = cfg.pushbullet.token;
      String body = "{\"nickname\":\"Matrix Clock\",\"model\":\"matrix-weather-clock\",\"manufacturer\":\"DIY\",\"icon\":\"system\",\"has_sms\":false}";
      JsonDocument doc(psramAllocator());
      String err;
      bool ok = http_util::postJson(String(API) + "devices", opt, body, [&](Stream& s, int) {
        return deserializeJson(doc, s) == DeserializationError::Ok;
      }, err);
      const char* iden = doc["iden"] | "";
      if (!ok || !*iden) { setErr(err.length() ? err : "device registration failed"); LOGW("pushbullet: %s", st.last_err); return false; }
      AppConfig next;
      app::cfgLock(); next = g_cfg; app::cfgUnlock();
      strlcpy(next.pushbullet.device_iden, iden, sizeof(next.pushbullet.device_iden));
      app::stageConfig(next, 0);          // persisted by the main loop
      st.device_ok = true;
      LOGI("pushbullet: registered device %s", iden);
      return true;
    }

    bool send(const AppConfig& cfg, const Item& it) {
      http_util::Options opt;
      opt.userAgent = MWC_USER_AGENT_NAME "/" MWC_VERSION;
      opt.headerName = "Access-Token"; opt.headerValue = cfg.pushbullet.token;
      opt.timeoutMs = 15000;
      String body = "{\"type\":\"note\",\"title\":\"" + jsonEscape(it.title) + "\",\"body\":\"" + jsonEscape(it.body) + "\"";
      if (cfg.pushbullet.device_iden[0]) body += String(",\"source_device_iden\":\"") + cfg.pushbullet.device_iden + "\"";
      body += "}";
      JsonDocument doc(psramAllocator());
      String err;
      int code = 0;
      bool ok = http_util::postJson(String(API) + "pushes", opt, body, [&](Stream& s, int) {
        return deserializeJson(doc, s) == DeserializationError::Ok;
      }, err, &code);
      if (!ok) { setErr(err); LOGW("pushbullet: send failed: %s", err.c_str()); return code != 401 && code != 403 && code < 500 ? false : false; }
      const char* iden = doc["iden"] | "";
      if (*iden) { ownIdens[ownIdx++ & 7] = iden; }
      st.sent++;
      LOGI("pushbullet: sent \"%s\"", it.title);
      return true;
    }
  }

  void begin() { mtx = xSemaphoreCreateMutex(); modifiedAfter = 0; }

  bool notify(const char* title, const char* body) {
    if (!g_cfg.pushbullet.token[0]) return false;
    if (!take()) return false;
    if (qcount < QUEUE_LEN) {
      Item& it = queue[(qhead + qcount) % QUEUE_LEN];
      strlcpy(it.title, title ? title : "Matrix Clock", sizeof(it.title));
      strlcpy(it.body, body ? body : "", sizeof(it.body));
      qcount++;
    }
    give();
    net_task::kick(net_task::JOB_PUSH);
    return true;
  }

  void runQueued(const AppConfig& cfg) {
    st.configured = cfg.pushbullet.token[0] != '\0';
    if (!st.configured) { if (take()) { qcount = 0; give(); } return; }
    if (!deviceChecked) { deviceChecked = ensureDevice(cfg); }
    for (uint8_t n = 0; n < QUEUE_LEN; n++) {
      Item it;
      if (!take()) return;
      if (!qcount) { give(); return; }
      it = queue[qhead];
      qhead = (qhead + 1) % QUEUE_LEN;
      qcount--;
      give();
      send(cfg, it);
    }
  }

  bool due(const AppConfig& cfg, uint32_t now_ms) {
    return cfg.pushbullet.token[0] && cfg.pushbullet.show_pushes && (int32_t)(now_ms - nextPoll) >= 0;
  }

  void poll(const AppConfig& cfg) {
    nextPoll = millis() + (uint32_t)cfg.pushbullet.poll_sec * 1000UL;
    st.configured = cfg.pushbullet.token[0] != '\0';
    if (!st.configured || !cfg.pushbullet.show_pushes) return;
    if (!deviceChecked) deviceChecked = ensureDevice(cfg);
    if (modifiedAfter == 0) {
      time_t now = time(nullptr);
      if (now < 1700000000) return;      // wait for a valid clock so old pushes are not replayed
      modifiedAfter = (double)now;
    }
    http_util::Options opt;
    opt.userAgent = MWC_USER_AGENT_NAME "/" MWC_VERSION;
    opt.headerName = "Access-Token"; opt.headerValue = cfg.pushbullet.token;
    opt.timeoutMs = 15000;
    String url = String(API) + "pushes?active=true&limit=5&modified_after=" + String(modifiedAfter, 3);
    JsonDocument doc(psramAllocator());
    JsonDocument filter;
    JsonObject f = filter["pushes"].add<JsonObject>();
    for (const char* k : { "iden", "type", "title", "body", "url", "modified", "dismissed", "target_device_iden", "source_device_iden" }) f[k] = true;
    String err;
    int code = 0;
    bool ok = http_util::get(url, opt, [&](Stream& s, int) {
      return deserializeJson(doc, s, DeserializationOption::Filter(filter)) == DeserializationError::Ok;
    }, err, &code);
    st.last_poll_ms = millis();
    if (!ok) { setErr(err); if (code == 429) nextPoll = millis() + 10 * 60000UL; return; }
    JsonArray pushes = doc["pushes"];
    // newest first: remember the highest modified stamp, show the oldest new push first
    for (int i = (int)pushes.size() - 1; i >= 0; i--) {
      JsonObject p = pushes[i];
      double mod = p["modified"] | 0.0;
      if (mod > modifiedAfter) modifiedAfter = mod;
      const char* iden = p["iden"] | "";
      bool own = false;
      for (uint8_t k = 0; k < 8; k++) if (ownIdens[k].length() && ownIdens[k] == iden) own = true;
      const char* src = p["source_device_iden"] | "";
      if (own || (cfg.pushbullet.device_iden[0] && strcmp(src, cfg.pushbullet.device_iden) == 0)) continue;
      const char* target = p["target_device_iden"] | "";
      if (*target && cfg.pushbullet.device_iden[0] && strcmp(target, cfg.pushbullet.device_iden) != 0) continue;   // meant for another device
      if (p["dismissed"] | false) continue;
      String text = p["title"] | "";
      const char* body = p["body"] | "";
      const char* link = p["url"] | "";
      if (*body) { if (text.length()) text += ": "; text += body; }
      else if (*link && !text.length()) text = link;
      if (!text.length()) continue;
      if (text.length() > 200) text = text.substring(0, 200);
      renderer::showMessage(text.c_str(), (uint32_t)cfg.pushbullet.show_sec * 1000UL, 0x40C0FF);
      if (cfg.pushbullet.chime) audio_out::chime(cfg.audio.chime, false);
      st.received++;
      LOGI("pushbullet: push \"%s\"", text.c_str());
    }
  }

  Status status() { st.configured = g_cfg.pushbullet.token[0] != '\0'; return st; }
}
