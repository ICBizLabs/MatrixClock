#include "radar.h"
#include <PNGdec.h>
#include <math.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "http_util.h"
#include "net_task.h"
#include "wifi_manager.h"
#include "util/log.h"
#include "version.h"

namespace radar {
  namespace {
    constexpr const char* BASE = "https://mesonet.agron.iastate.edu/cgi-bin/wms/nexrad/n0r.cgi?SERVICE=WMS&VERSION=1.1.1&REQUEST=GetMap&STYLES=&SRS=EPSG:4326&FORMAT=image/png&TRANSPARENT=FALSE&BGCOLOR=0x000000";
    constexpr size_t MAX_PNG = 24 * 1024;
    constexpr uint32_t DUP_RETRY_MS = 60000UL;
    constexpr size_t FRAME_PX = (size_t)W * H;

    SemaphoreHandle_t mtx = nullptr;
    uint16_t* frames = nullptr;                 // MAX_FRAMES * FRAME_PX, oldest first
    uint32_t frameT[MAX_FRAMES];                // millis() the composite is valid for (approximate)
    uint8_t count = 0;
    uint8_t* pngBuf = nullptr;
    uint16_t* work = nullptr;                   // decode target
    PNG* png = nullptr;
    Status st = {};
    uint32_t nextFetch = 0;
    volatile bool refreshRequested = false, resetRequested = false;
    int pngW = 0, pngH = 0;

    bool take() { return mtx && xSemaphoreTake(mtx, pdMS_TO_TICKS(100)) == pdTRUE; }
    void give() { xSemaphoreGive(mtx); }

    int drawLine(PNGDRAW* d) {   // returns 1 to keep decoding
      // nearest-neighbour into the 64x32 work frame in case the server returns another size
      static uint16_t line[1024];
      if (d->iWidth > 1024) return 0;
      PNG* p = (PNG*)d->pUser;
      p->getLineAsRGB565(d, line, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
      if (pngW == W && pngH == H) { if (d->y < H) memcpy(work + d->y * W, line, W * 2); return 1; }
      int y0 = (int)((int32_t)d->y * H / pngH), y1 = (int)((int32_t)(d->y + 1) * H / pngH);
      if (y1 <= y0) y1 = y0 + 1;
      for (int y = y0; y < y1 && y < H; y++)
        for (int x = 0; x < W; x++) work[y * W + x] = line[(int)((int32_t)x * pngW / W)];
      return 1;
    }

    String url(const AppConfig& cfg, const char* layerSuffix) {
      const float r = (float)cfg.radar.radius_km;
      const float lat = cfg.location.lat, lon = cfg.location.lon;
      const float dlat = r / 111.32f, dlon = r / (111.32f * cosf(lat * (float)M_PI / 180.0f));
      char bbox[96];
      snprintf(bbox, sizeof(bbox), "%.4f,%.4f,%.4f,%.4f", lon - dlon, lat - dlat / 2, lon + dlon, lat + dlat / 2);   // 2r wide, r tall
      String u = BASE;
      u += "&LAYERS=nexrad-n0r"; u += layerSuffix;
      u += "&BBOX="; u += bbox;
      u += "&WIDTH="; u += W; u += "&HEIGHT="; u += H;
      return u;
    }

    bool fetchLayer(const AppConfig& cfg, const char* suffix, String& err) {
      http_util::Options opt;
      opt.userAgent = MWC_USER_AGENT_NAME "/" MWC_VERSION;
      opt.accept = "image/png";
      opt.timeoutMs = 20000;
      size_t got = 0;
      int code = 0;
      bool ok = http_util::get(url(cfg, suffix), opt, [&](Stream& s, int len) -> bool {
        // the map server answers HTTP/1.0 requests without a Content-Length and closes the connection after the body
        return http_util::readBody(s, len, pngBuf, MAX_PNG, got, err);
      }, err, &code);
      if (!ok) return false;
      if (got < 8 || memcmp(pngBuf, "\x89PNG", 4) != 0) { err = "not a PNG (service error?)"; return false; }
      memset(work, 0, FRAME_PX * 2);
      int rc = png->openRAM(pngBuf, (int)got, drawLine);
      if (rc != PNG_SUCCESS) { err = "png open failed"; return false; }
      pngW = png->getWidth(); pngH = png->getHeight();
      rc = png->decode(png, 0);
      png->close();
      if (rc != PNG_SUCCESS) { err = String("png decode error ") + rc; return false; }
      return true;
    }

    void analyse() {   // newest frame: echo statistics (caller holds the mutex)
      st.echo_near = false; st.echo_pct = 0;
      if (!count) return;
      const uint16_t* f = frames + (count - 1) * FRAME_PX;
      uint32_t all = 0, near = 0;
      for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        if (!f[y * W + x]) continue;
        all++;
        if (x >= W / 3 && x < 2 * W / 3 && y >= H / 3 && y < 2 * H / 3) near++;
      }
      st.echo_pct = (uint8_t)(all * 100 / FRAME_PX);
      st.echo_near = near >= 3;
    }

    void push(uint32_t validAt) {   // work -> newest frame
      if (!take()) return;
      if (count == MAX_FRAMES) { memmove(frames, frames + FRAME_PX, (MAX_FRAMES - 1) * FRAME_PX * 2); memmove(frameT, frameT + 1, (MAX_FRAMES - 1) * sizeof(uint32_t)); count--; }
      memcpy(frames + count * FRAME_PX, work, FRAME_PX * 2);
      frameT[count] = validAt;
      count++;
      analyse();
      give();
    }
  }

  void begin() {
    mtx = xSemaphoreCreateMutex();
    frames = (uint16_t*)heap_caps_calloc(MAX_FRAMES * FRAME_PX, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    work = (uint16_t*)heap_caps_malloc(FRAME_PX * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    pngBuf = (uint8_t*)heap_caps_malloc(MAX_PNG, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    png = new PNG();
    if (!frames || !work || !pngBuf || !png) LOGE("radar: no memory");
    nextFetch = millis() + 20000UL;
  }

  void applyConfig() { resetRequested = true; net_task::kick(net_task::JOB_RADAR); }
  void requestRefresh() { refreshRequested = true; net_task::kick(net_task::JOB_RADAR); }

  bool due(const AppConfig& cfg, uint32_t now) {
    if (!cfg.radar.enabled || !frames) return false;
    return refreshRequested || resetRequested || (int32_t)(now - nextFetch) >= 0;
  }

  void run(const AppConfig& cfg) {
    if (!frames || !png) return;
    st.enabled = cfg.radar.enabled;
    if (resetRequested) { resetRequested = false; if (take()) { count = 0; analyse(); give(); } }
    if (!cfg.radar.enabled || !wifi_mgr::isConnected()) return;
    refreshRequested = false;
    const uint32_t now = millis();
    const uint32_t period = (uint32_t)(cfg.radar.refresh_min ? cfg.radar.refresh_min : 5) * 60000UL;
    String err;
    bool ok = true;
    if (count == 0) {
      // first fill: the 50..5 minutes-ago layers, then the current composite
      static const char* const PAST[10] = { "-m50m", "-m45m", "-m40m", "-m35m", "-m30m", "-m25m", "-m20m", "-m15m", "-m10m", "-m05m" };
      for (int i = 0; i < 10 && ok; i++) {
        if (!fetchLayer(cfg, PAST[i], err)) { ok = false; break; }
        push(now - (uint32_t)(50 - 5 * i) * 60000UL);
        delay(50);
      }
      if (ok && fetchLayer(cfg, "", err)) push(now); else ok = false;
    } else {
      if (fetchLayer(cfg, "", err)) {
        bool same = false;
        if (take()) { same = memcmp(work, frames + (count - 1) * FRAME_PX, FRAME_PX * 2) == 0; give(); }
        if (same) { nextFetch = now + DUP_RETRY_MS; if (take()) { st.last_ok_ms = now; give(); } return; }   // composite not updated yet
        push(now);
      } else ok = false;
    }
    if (take()) {
      if (ok) { st.last_ok_ms = now; st.fails = 0; st.err[0] = '\0'; }
      else { st.last_err_ms = now; if (st.fails < 10) st.fails++; strlcpy(st.err, err.c_str(), sizeof(st.err)); }
      st.frames = count;
      give();
    }
    if (ok) { nextFetch = now + period; LOGI("radar: %u frames, echoes %u%%%s", count, st.echo_pct, st.echo_near ? " nearby" : ""); }
    else { nextFetch = now + min<uint32_t>(period * st.fails, 30 * 60000UL); LOGW("radar: %s (retry in %lu min)", err.c_str(), (unsigned long)((nextFetch - now) / 60000UL)); }
  }

  Status status() { Status c = {}; if (take()) { c = st; c.frames = count; give(); } return c; }
  uint8_t frameCount() { return count; }
  int32_t frameAgeMin(uint8_t i) {
    int32_t a = -1;
    if (take()) { if (i < count) a = (int32_t)((millis() - frameT[i]) / 60000UL); give(); }
    return a;
  }
  bool copyFrame(uint8_t i, uint16_t* out) {
    if (!take()) return false;
    bool ok = i < count;
    if (ok) memcpy(out, frames + i * FRAME_PX, FRAME_PX * 2);
    give();
    return ok;
  }
  size_t copyFrameBytes(uint8_t i, uint8_t* out, size_t offset, size_t maxLen) {
    if (!take()) return 0;
    size_t n = 0;
    if (i < count && offset < FRAME_PX * 2) {
      n = min(maxLen, FRAME_PX * 2 - offset);
      memcpy(out, (const uint8_t*)(frames + i * FRAME_PX) + offset, n);
    }
    give();
    return n;
  }
  bool echoNearby() { bool e = false; if (take()) { e = count > 0 && st.echo_near; give(); } return e; }
}
