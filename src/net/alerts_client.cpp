#include "alerts_client.h"
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include "alert_store.h"
#include "http_util.h"
#include "util/psram_alloc.h"
#include "util/log.h"
#include "version.h"
#include "util/timeutil.h"

namespace alerts_client {
  namespace {
    uint8_t urgencyCode(const char* u) {
      if (!u) return 0;
      if (!strcasecmp(u, "Immediate")) return 4;
      if (!strcasecmp(u, "Expected")) return 3;
      if (!strcasecmp(u, "Future")) return 2;
      if (!strcasecmp(u, "Past")) return 1;
      return 0;
    }
    String caCert;   // loaded lazily from /nws_root.pem when tls_verify is on
  }

  bool parseIso8601(const char* s, time_t& out) {
    if (!s || strlen(s) < 19) return false;
    int Y, M, D, h, m, sec;
    if (sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d", &Y, &M, &D, &h, &m, &sec) != 6) return false;
    int64_t t = days_from_civil(Y, M, D) * 86400 + h * 3600 + m * 60 + sec;
    const char* p = s + 19;
    if (*p == '.') { while (*p && *p != 'Z' && *p != '+' && *p != '-') p++; }   // skip fractional seconds
    if (*p == '+' || *p == '-') {
      int oh = 0, om = 0;
      if (sscanf(p + 1, "%2d:%2d", &oh, &om) >= 1) {
        int off = oh * 3600 + om * 60;
        t -= (*p == '+') ? off : -off;
      }
    }
    out = (time_t)t;
    return true;
  }

  String buildUrl(const AppConfig& cfg) {
    String url = "https://api.weather.gov/alerts/active?point=";
    url += String(cfg.location.lat, 4);
    url += ",";
    url += String(cfg.location.lon, 4);
    url += "&status=actual&message_type=alert,update";   // note: /alerts/active rejects a limit parameter
    return url;
  }

  bool fetch(const AppConfig& cfg, String& err) {
    if (cfg.alerts.tls_verify && caCert.length() == 0) {
      File f = LittleFS.open("/nws_root.pem", "r");
      if (f) { caCert = f.readString(); f.close(); }
      else { err = "tls_verify on but /nws_root.pem missing"; return false; }
    }
    JsonDocument doc(psramAllocator());
    JsonDocument filter;
    JsonObject f = filter["features"].add<JsonObject>();
    f["id"] = true;
    JsonObject p = f["properties"].to<JsonObject>();
    for (const char* k : { "id", "event", "severity", "urgency", "certainty", "messageType", "headline",
                           "onset", "ends", "expires", "senderName", "areaDesc" }) p[k] = true;
    p["references"][0]["identifier"] = true;

    String ua = String(MWC_USER_AGENT_NAME "/" MWC_VERSION " (") + (cfg.alerts.user_agent_contact[0] ? cfg.alerts.user_agent_contact : "no-contact") + ")";
    http_util::Options opt;
    opt.userAgent = ua.c_str();
    opt.accept = "application/geo+json";
    opt.tlsVerify = cfg.alerts.tls_verify;
    opt.caCert = caCert.length() ? caCert.c_str() : nullptr;
    opt.timeoutMs = 15000;
    int code = 0;
    bool ok = http_util::get(buildUrl(cfg), opt, [&](Stream& s, int) {
      DeserializationError e = deserializeJson(doc, s, DeserializationOption::Filter(filter), DeserializationOption::NestingLimit(16));
      if (e) { err = String("json: ") + e.c_str(); return false; }
      return true;
    }, err, &code);
    if (!ok) return false;

    JsonArray feats = doc["features"];
    size_t n = 0;
    AlertItem* fresh = (AlertItem*)heap_caps_calloc(MAX_ALERTS, sizeof(AlertItem), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!fresh) fresh = (AlertItem*)calloc(MAX_ALERTS, sizeof(AlertItem));
    if (!fresh) { err = "no memory"; return false; }
    for (JsonObject ft : feats) {
      if (n >= MAX_ALERTS) break;
      JsonObject pr = ft["properties"];
      const char* id = pr["id"] | (const char*)(ft["id"] | "");
      if (!id || !*id) continue;
      AlertItem& a = fresh[n];
      strlcpy(a.id, id, sizeof(a.id));
      strlcpy(a.event, pr["event"] | "Alert", sizeof(a.event));
      strlcpy(a.headline, pr["headline"] | a.event, sizeof(a.headline));
      strlcpy(a.sender, pr["senderName"] | "", sizeof(a.sender));
      strlcpy(a.area, pr["areaDesc"] | "", sizeof(a.area));
      strlcpy(a.msgType, pr["messageType"] | "Alert", sizeof(a.msgType));
      a.ref[0] = '\0';
      JsonArray refs = pr["references"];
      if (!refs.isNull() && refs.size() > 0) strlcpy(a.ref, refs[0]["identifier"] | "", sizeof(a.ref));
      Severity sev;
      a.sev = severity_parse(pr["severity"] | "Unknown", sev) ? sev : Severity::Unknown;
      a.urgency = urgencyCode(pr["urgency"] | "");
      if (!parseIso8601(pr["onset"] | "", a.onset)) a.onset = 0;
      if (!parseIso8601(pr["ends"] | "", a.ends)) a.ends = 0;
      if (!parseIso8601(pr["expires"] | "", a.expires)) a.expires = 0;
      n++;
    }
    alerts::applySnapshot(fresh, n, millis());
    free(fresh);
    LOGI("alerts: poll ok, %u active feature(s)", (unsigned)n);
    return true;
  }
}
