#pragma once
#include <Arduino.h>
#include <functional>

// One-shot HTTP(S) GET that hands the body stream to a consumer. Uses HTTP/1.0 (no chunked encoding) so the
// stream can be fed straight into ArduinoJson. A fresh client is created per call so TLS memory is released.
namespace http_util {
  struct Options {
    const char* userAgent = "matrix-weather-clock";
    const char* accept = nullptr;
    bool tlsVerify = false;
    const char* caCert = nullptr;     // PEM, used when tlsVerify is true
    uint32_t timeoutMs = 10000;
    const char* headerName = nullptr;   // one extra request header (e.g. Access-Token)
    const char* headerValue = nullptr;
  };
  // consume() returns true on success. err receives a short message on failure. httpCode is set when a response arrived.
  bool get(const String& url, const Options& opt, std::function<bool(Stream&, int contentLength)> consume, String& err, int* httpCode = nullptr);
  // JSON POST; the response body (if any) is handed to consume when provided
  bool postJson(const String& url, const Options& opt, const String& body, std::function<bool(Stream&, int contentLength)> consume, String& err, int* httpCode = nullptr);
  bool putJson(const String& url, const Options& opt, const String& body, std::function<bool(Stream&, int contentLength)> consume, String& err, int* httpCode = nullptr);
  bool enoughMemoryForTls();          // largest free internal block check before a TLS handshake
  // Reads a whole response body into buf inside a consume callback. Handles both a known Content-Length and the
  // HTTP/1.0 "Connection: close" case (contentLength < 0, body ends when the server closes). Fails on more than max bytes.
  bool readBody(Stream& s, int contentLength, uint8_t* buf, size_t max, size_t& got, String& err, uint32_t stallMs = 10000);
}
