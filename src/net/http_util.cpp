#include "http_util.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <memory>
#include "util/log.h"

namespace http_util {
  // The Arduino core builds mbedtls with CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC: the two 16 KB TLS record buffers and the
  // handshake state must come from internal RAM, about 40 KB per connection. Below this the connect fails as
  // "connection refused", so it is better to skip and say why.
  bool enoughMemoryForTls() {
    size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t freeInt = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (largest >= 18 * 1024 && freeInt >= 46 * 1024) return true;
    LOGW("tls: skipped, largest internal block %u B (free %u B, psram %u B)", (unsigned)largest,
         (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT), (unsigned)ESP.getFreePsram());
    return false;
  }

  namespace {
    bool request(const String& url, const Options& opt, const String* body, std::function<bool(Stream&, int)> consume, String& err, int* httpCode, bool put = false);
  }

  bool readBody(Stream& s, int len, uint8_t* buf, size_t max, size_t& got, String& err, uint32_t stallMs) {
    NetworkClient& c = static_cast<NetworkClient&>(s);   // HTTPClient::getStream() hands out its NetworkClient
    got = 0;
    if (len > 0 && (size_t)len > max) { err = "body too large"; return false; }
    uint32_t last = millis();
    for (;;) {
      if (len > 0 && got >= (size_t)len) break;
      int avail = c.available();
      if (avail > 0) {
        size_t want = (size_t)avail < max - got ? (size_t)avail : max - got;
        if (!want) { err = "body too large"; return false; }
        int n = c.read(buf + got, want);
        if (n > 0) { got += (size_t)n; last = millis(); continue; }
      }
      if (!c.connected() && c.available() <= 0) break;          // HTTP/1.0: the server closes after the body
      if (millis() - last > stallMs) { err = "download stalled"; return false; }
      delay(1);
    }
    if (len > 0 && got != (size_t)len) { err = "short body"; return false; }
    if (!got) { err = "empty body"; return false; }
    return true;
  }

  bool get(const String& url, const Options& opt, std::function<bool(Stream&, int)> consume, String& err, int* httpCode) {
    return request(url, opt, nullptr, consume, err, httpCode);
  }

  bool postJson(const String& url, const Options& opt, const String& body, std::function<bool(Stream&, int)> consume, String& err, int* httpCode) {
    return request(url, opt, &body, consume, err, httpCode);
  }

  bool putJson(const String& url, const Options& opt, const String& body, std::function<bool(Stream&, int)> consume, String& err, int* httpCode) {
    return request(url, opt, &body, consume, err, httpCode, true);
  }

  namespace {
  bool request(const String& url, const Options& opt, const String* body, std::function<bool(Stream&, int)> consume, String& err, int* httpCode, bool put) {
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
    if (body) { http.addHeader("Content-Type", "application/json"); code = put ? http.PUT(*body) : http.POST(*body); }
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
