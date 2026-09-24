#include "http_util.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <memory>
#include "util/log.h"

namespace http_util {
  // The 16 KB TLS record buffers are allocated in PSRAM; the handshake still needs a few contiguous KB of internal RAM.
  bool enoughMemoryForTls() {
    size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (largest >= 12 * 1024) return true;
    LOGW("tls: skipped, largest internal block %u B (free %u B, psram %u B)", (unsigned)largest,
         (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT), (unsigned)ESP.getFreePsram());
    return false;
  }

  namespace {
    bool request(const String& url, const Options& opt, const String* body, std::function<bool(Stream&, int)> consume, String& err, int* httpCode);
  }

  bool get(const String& url, const Options& opt, std::function<bool(Stream&, int)> consume, String& err, int* httpCode) {
    return request(url, opt, nullptr, consume, err, httpCode);
  }

  bool postJson(const String& url, const Options& opt, const String& body, std::function<bool(Stream&, int)> consume, String& err, int* httpCode) {
    return request(url, opt, &body, consume, err, httpCode);
  }

  namespace {
  bool request(const String& url, const Options& opt, const String* body, std::function<bool(Stream&, int)> consume, String& err, int* httpCode) {
    const bool https = url.startsWith("https://");
    std::unique_ptr<WiFiClient> client;
    if (https) {
      if (!enoughMemoryForTls()) { err = "low memory for TLS"; return false; }
      auto* sc = new WiFiClientSecure();
      if (opt.tlsVerify && opt.caCert) sc->setCACert(opt.caCert);
      else sc->setInsecure();
      sc->setHandshakeTimeout(15);
      client.reset(sc);
    } else {
      client.reset(new WiFiClient());
    }
    HTTPClient http;
    http.setReuse(false);
    http.useHTTP10(true);
    http.setTimeout(opt.timeoutMs);
    http.setConnectTimeout(opt.timeoutMs);
    http.setUserAgent(opt.userAgent);
    if (!http.begin(*client, url)) { err = "bad url"; return false; }
    if (opt.accept) http.addHeader("Accept", opt.accept);
    if (opt.headerName && opt.headerValue) http.addHeader(opt.headerName, opt.headerValue);
    http.addHeader("Accept-Encoding", "identity");
    int code;
    if (body) { http.addHeader("Content-Type", "application/json"); code = http.POST(*body); }
    else code = http.GET();
    if (httpCode) *httpCode = code;
    bool ok = false;
    if (code == HTTP_CODE_OK) {
      ok = consume ? consume(http.getStream(), http.getSize()) : true;
      if (!ok && err.length() == 0) err = "bad response body";
    } else if (code > 0) {
      err = String("http ") + code;
    } else {
      err = String("conn: ") + HTTPClient::errorToString(code);
    }
    http.end();
    return ok;
  }
  }
}
