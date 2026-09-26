#include "wifi_manager.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include "util/log.h"

namespace wifi_mgr {
  namespace {
    enum class State : uint8_t { Idle, Connecting, Connected, Ap };
    State state = State::Idle;
    WifiConfig cfg;
    DNSServer dns;
    bool apUp = false;
    bool mdnsUp = false;
    bool connectedEvent = false;
    uint32_t stateSince = 0;
    uint32_t lastStaRetry = 0;
    uint32_t apDropAt = 0;
    String apName;
    const IPAddress AP_IP(4, 3, 2, 1);
    constexpr uint32_t CONNECT_TIMEOUT_MS = 20000;
    constexpr uint32_t STA_RETRY_MS = 60000;

    void startSta() {
      if (!cfg.ssid[0]) return;
      WiFi.setHostname(cfg.hostname);
      WiFi.setSleep(false);
      WiFi.setAutoReconnect(false);
      WiFi.begin(cfg.ssid, cfg.pass);
      WiFi.setTxPower((wifi_power_t)cfg.tx_power);
      state = State::Connecting;
      stateSince = millis();
      lastStaRetry = millis();
      LOGI("wifi: connecting to %s", cfg.ssid);
    }

    void startAp() {
      if (apUp) return;
      uint8_t mac[6];
      WiFi.macAddress(mac);
      char name[24];
      snprintf(name, sizeof(name), "MatrixWeatherClock-%02X%02X", mac[4], mac[5]);
      apName = name;
      WiFi.mode(cfg.ssid[0] ? WIFI_AP_STA : WIFI_AP);
      WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
      WiFi.softAP(apName.c_str(), cfg.ap_pass[0] ? cfg.ap_pass : nullptr);
      dns.setErrorReplyCode(DNSReplyCode::NoError);
      dns.start(53, "*", AP_IP);
      apUp = true;
      state = State::Ap;
      stateSince = millis();
      LOGI("wifi: AP %s at %s%s", apName.c_str(), AP_IP.toString().c_str(), cfg.ssid[0] ? " (still retrying STA)" : "");
    }

    void stopAp() {
      if (!apUp) return;
      dns.stop();
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      apUp = false;
      LOGI("wifi: AP stopped");
    }

    void onConnected() {
      state = State::Connected;
      stateSince = millis();
      connectedEvent = true;
      LOGI("wifi: connected, ip %s rssi %d", WiFi.localIP().toString().c_str(), WiFi.RSSI());
      if (!mdnsUp) {
        if (MDNS.begin(cfg.hostname)) { MDNS.addService("http", "tcp", 80); mdnsUp = true; }
        else LOGW("wifi: mDNS start failed");
      }
      if (apUp) apDropAt = millis() + 10000;
    }
  }

  void begin(const WifiConfig& wc) {
    cfg = wc;
    WiFi.persistent(false);
    if (cfg.ssid[0]) { WiFi.mode(WIFI_STA); startSta(); }
    else startAp();
  }

  void loop() {
    uint32_t now = millis();
    if (apUp) dns.processNextRequest();
    wl_status_t st = WiFi.status();
    switch (state) {
      case State::Connecting:
        if (st == WL_CONNECTED) onConnected();
        else if (now - stateSince > CONNECT_TIMEOUT_MS) { LOGW("wifi: connect timeout"); startAp(); }
        break;
      case State::Connected:
        if (st != WL_CONNECTED) { LOGW("wifi: link lost, reconnecting"); mdnsUp = false; MDNS.end(); WiFi.disconnect(); startSta(); }
        else if (apUp && apDropAt && (int32_t)(now - apDropAt) >= 0) { stopAp(); apDropAt = 0; }
        break;
      case State::Ap:
        if (st == WL_CONNECTED) onConnected();
        else if (cfg.ssid[0] && now - lastStaRetry > STA_RETRY_MS) { lastStaRetry = now; WiFi.begin(cfg.ssid, cfg.pass); LOGI("wifi: retrying STA"); }
        break;
      default: break;
    }
  }

  void applyCredentials(const WifiConfig& wc) {
    cfg = wc;
    mdnsUp = false;
    MDNS.end();
    WiFi.disconnect();
    WiFi.mode(apUp ? WIFI_AP_STA : WIFI_STA);
    startSta();
  }

  bool isConnected() { return WiFi.status() == WL_CONNECTED; }
  bool apActive() { return apUp; }
  String apSsid() { return apName; }
  IPAddress ip() { return isConnected() ? WiFi.localIP() : (apUp ? AP_IP : IPAddress()); }
  int8_t rssi() { return isConnected() ? (int8_t)WiFi.RSSI() : 0; }
  bool consumeConnectedEvent() { bool e = connectedEvent; connectedEvent = false; return e; }

  void startScan() {
    if (WiFi.scanComplete() != WIFI_SCAN_RUNNING) WiFi.scanNetworks(true, false, false, 300);
  }

  int scanToJson(JsonArray out) {
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return -1;
    if (n < 0) return 0;
    for (int i = 0; i < n && i < 20; i++) {
      JsonObject o = out.add<JsonObject>();
      o["ssid"] = WiFi.SSID(i);
      o["rssi"] = WiFi.RSSI(i);
      o["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    return n;
  }

  bool isCaptiveRequest(const String& host) {
    if (!apUp) return false;
    return host != AP_IP.toString() && host != String(cfg.hostname) + ".local";
  }
}
