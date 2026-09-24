#include "web_server.h"
#include <Update.h>
#include "generated/web_assets.h"
#include "app.h"
#include "net/wifi_manager.h"
#include "net/net_task.h"
#include "display/renderer.h"
#include "util/log.h"

namespace web {
  namespace {
    AsyncWebServer server(80);
    size_t otaReceived = 0;

    void sendGz(AsyncWebServerRequest* r, const uint8_t* data, size_t len, const char* etag) {
      if (r->hasHeader("If-None-Match") && r->header("If-None-Match") == etag) { r->send(304); return; }
      AsyncWebServerResponse* res = r->beginResponse(200, "text/html", data, len);
      res->addHeader("Content-Encoding", "gzip");
      res->addHeader("ETag", etag);
      res->addHeader("Cache-Control", "no-cache");
      r->send(res);
    }
    void sendIndex(AsyncWebServerRequest* r) { sendGz(r, WEB_INDEX_GZ, WEB_INDEX_GZ_LEN, WEB_INDEX_ETAG); }
    void sendSetup(AsyncWebServerRequest* r) { sendGz(r, WEB_SETUP_GZ, WEB_SETUP_GZ_LEN, WEB_SETUP_ETAG); }
    void sendStatic(AsyncWebServerRequest* r, const char* type, const uint8_t* data, size_t len) {
      AsyncWebServerResponse* res = r->beginResponse(200, type, data, len);
      res->addHeader("Cache-Control", "max-age=86400");
      r->send(res);
    }

    // the setup access point sends phones and laptops to the wizard
    void captiveRedirect(AsyncWebServerRequest* r) { r->redirect("http://4.3.2.1/setup"); }

    void otaRequest(AsyncWebServerRequest* r) {
      bool ok = !Update.hasError() && Update.isFinished();
      String body = ok ? "{\"ok\":true}" : String("{\"ok\":false,\"error\":\"") + Update.errorString() + "\"}";
      AsyncWebServerResponse* res = r->beginResponse(ok ? 200 : 500, "application/json", body);
      res->addHeader("Connection", "close");
      r->send(res);
      if (ok) { LOGI("ota: success, rebooting"); app::requestReboot(800); }
      else { LOGE("ota: failed: %s", Update.errorString()); net_task::pause(false); renderer::setOta(false, 0); }
    }

    void otaUpload(AsyncWebServerRequest* r, String filename, size_t index, uint8_t* data, size_t len, bool final) {
      if (index == 0) {
        LOGI("ota: start %s", filename.c_str());
        net_task::pause(true);
        renderer::setOta(true, 0);
        otaReceived = 0;
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) LOGE("ota: begin failed: %s", Update.errorString());
      }
      if (len && !Update.hasError()) {
        if (Update.write(data, len) != len) LOGE("ota: write failed: %s", Update.errorString());
        otaReceived += len;
        size_t total = r->contentLength();
        renderer::setOta(true, total ? (uint8_t)min<size_t>(99, otaReceived * 100 / total) : 50);
      }
      if (final && !Update.hasError()) {
        if (Update.end(true)) { renderer::setOta(true, 100); LOGI("ota: %u bytes written", (unsigned)otaReceived); }
        else LOGE("ota: end failed: %s", Update.errorString());
      }
    }
  }

  void sendJsonError(AsyncWebServerRequest* r, int code, const String& msg) {
    String body = "{\"ok\":false,\"error\":\"";
    for (size_t i = 0; i < msg.length(); i++) { char c = msg[i]; if (c == '"' || c == '\\') body += '\\'; body += c; }
    body += "\"}";
    r->send(code, "application/json", body);
  }

  void begin() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) {
      if (wifi_mgr::isCaptiveRequest(r->host())) { captiveRedirect(r); return; }
      sendIndex(r);
    });
    server.on("/index.html", HTTP_GET, sendIndex);
    server.on("/setup", HTTP_GET, sendSetup);
    server.on("/manifest.webmanifest", HTTP_GET, [](AsyncWebServerRequest* r) { sendStatic(r, "application/manifest+json", (const uint8_t*)WEB_MANIFEST, strlen_P(WEB_MANIFEST)); });
    server.on("/sw.js", HTTP_GET, [](AsyncWebServerRequest* r) { sendStatic(r, "text/javascript", (const uint8_t*)WEB_SW_JS, strlen_P(WEB_SW_JS)); });
    server.on("/icon-192.png", HTTP_GET, [](AsyncWebServerRequest* r) { sendStatic(r, "image/png", WEB_ICON_192, WEB_ICON_192_LEN); });
    server.on("/icon-512.png", HTTP_GET, [](AsyncWebServerRequest* r) { sendStatic(r, "image/png", WEB_ICON_512, WEB_ICON_512_LEN); });
    server.on("/apple-touch-icon.png", HTTP_GET, [](AsyncWebServerRequest* r) { sendStatic(r, "image/png", WEB_ICON_192, WEB_ICON_192_LEN); });
    // captive portal probes used by phones and desktops
    for (const char* p : { "/generate_204", "/gen_204", "/hotspot-detect.html", "/connecttest.txt", "/ncsi.txt", "/fwlink",
                           "/redirect", "/canonical.html", "/success.txt", "/library/test/success.html" }) {
      server.on(p, HTTP_GET, [](AsyncWebServerRequest* r) {
        if (wifi_mgr::apActive()) captiveRedirect(r); else r->send(404);
      });
    }
    server.on("/update", HTTP_POST, otaRequest, otaUpload);
    registerApi(server);
    server.onNotFound([](AsyncWebServerRequest* r) {
      if (wifi_mgr::isCaptiveRequest(r->host())) { captiveRedirect(r); return; }
      r->send(404, "text/plain", "not found");
    });
    server.begin();
    LOGI("web: server started");
  }
}
