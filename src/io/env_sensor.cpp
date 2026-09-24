#include "env_sensor.h"
#include <math.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "i2c_bus.h"
#include "util/log.h"

namespace env_sensor {
  namespace {
    constexpr uint8_t ADDRS[2] = { 0x76, 0x77 };
    constexpr uint8_t REG_ID = 0xD0;
    constexpr uint16_t HIST_MIN = 24 * 60;               // one point per minute
    constexpr uint32_t MEAS_WAIT_MS = 80;
    // trend thresholds over the configured windows
    constexpr float TEMP_STEP = 0.5f, TEMP_FAST = 1.5f;          // C
    constexpr float HUM_STEP = 3.0f, HUM_FAST = 8.0f;            // % RH
    constexpr float PRESS_STEP = 1.0f, PRESS_FAST = 3.0f;        // hPa over the pressure window (WMO: 3 h)

    struct Hist { float t, h, p; };
    IndoorConfig cfg;
    Type kind = Type::None;
    uint8_t addr = 0;
    SemaphoreHandle_t mtx = nullptr;
    Reading cur;
    Hist* hist = nullptr;                                 // PSRAM ring, HIST_MIN entries
    uint16_t histHead = 0, histCount = 0;
    uint32_t lastPoint = 0, lastTrigger = 0, readyAt = 0, errs = 0;
    bool measuring = false;
    float altHint = NAN;
    float rawT = 0, rawH = 0, rawP = 0;                   // last compensated values before offsets

    // ---- BME280 / BMP280 calibration ----
    struct { uint16_t T1; int16_t T2, T3; uint16_t P1; int16_t P2, P3, P4, P5, P6, P7, P8, P9; uint8_t H1; int16_t H2; uint8_t H3; int16_t H4, H5; int8_t H6; } c280;
    // ---- BME680 calibration ----
    struct { uint16_t T1; int16_t T2; int8_t T3; uint16_t P1; int16_t P2; int8_t P3; int16_t P4, P5; int8_t P6, P7; int16_t P8, P9; uint8_t P10;
             uint16_t H1, H2; int8_t H3, H4, H5; uint8_t H6; int8_t H7; } c680;

    bool take() { return mtx && xSemaphoreTake(mtx, pdMS_TO_TICKS(20)) == pdTRUE; }
    void give() { xSemaphoreGive(mtx); }
    inline uint16_t u16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
    inline int16_t s16(const uint8_t* p) { return (int16_t)u16(p); }

    bool init280() {
      uint8_t c[26], h[7];
      if (!i2c_bus::readRegs(addr, 0x88, c, 26)) return false;
      c280.T1 = u16(c); c280.T2 = s16(c + 2); c280.T3 = s16(c + 4);
      c280.P1 = u16(c + 6); c280.P2 = s16(c + 8); c280.P3 = s16(c + 10); c280.P4 = s16(c + 12); c280.P5 = s16(c + 14);
      c280.P6 = s16(c + 16); c280.P7 = s16(c + 18); c280.P8 = s16(c + 20); c280.P9 = s16(c + 22);
      c280.H1 = c[25];
      if (kind == Type::BME280) {
        if (!i2c_bus::readRegs(addr, 0xE1, h, 7)) return false;
        c280.H2 = s16(h); c280.H3 = h[2];
        c280.H4 = (int16_t)((h[3] << 4) | (h[4] & 0x0F));
        c280.H5 = (int16_t)((h[5] << 4) | (h[4] >> 4));
        c280.H6 = (int8_t)h[6];
        i2c_bus::writeReg(addr, 0xF2, 0x01);           // humidity oversampling x1
      }
      return i2c_bus::writeReg(addr, 0xF5, 0x00);      // filter off, sleep between forced measurements
    }

    bool init680() {
      uint8_t a[23], b[14];
      if (!i2c_bus::readRegs(addr, 0x8A, a, 23) || !i2c_bus::readRegs(addr, 0xE1, b, 14)) return false;
      c680.T2 = s16(a); c680.T3 = (int8_t)a[2];
      c680.P1 = u16(a + 4); c680.P2 = s16(a + 6); c680.P3 = (int8_t)a[8]; c680.P4 = s16(a + 10); c680.P5 = s16(a + 12);
      c680.P7 = (int8_t)a[14]; c680.P6 = (int8_t)a[15]; c680.P10 = a[18]; c680.P8 = s16(a + 19); c680.P9 = s16(a + 21);
      c680.H2 = (uint16_t)((b[0] << 4) | (b[1] >> 4));
      c680.H1 = (uint16_t)((b[2] << 4) | (b[1] & 0x0F));
      c680.H3 = (int8_t)b[3]; c680.H4 = (int8_t)b[4]; c680.H5 = (int8_t)b[5]; c680.H6 = b[6]; c680.H7 = (int8_t)b[7];
      c680.T1 = u16(b + 8);
      bool ok = i2c_bus::writeReg(addr, 0x72, 0x01);   // humidity oversampling x1
      ok &= i2c_bus::writeReg(addr, 0x70, 0x08);       // heater off
      ok &= i2c_bus::writeReg(addr, 0x71, 0x00);       // gas measurement off
      ok &= i2c_bus::writeReg(addr, 0x75, 0x00);       // filter off
      return ok;
    }

    bool trigger() {
      // temperature x2, pressure x4, forced mode
      return i2c_bus::writeReg(addr, kind == Type::BME680 ? 0x74 : 0xF4, (2 << 5) | (4 << 2) | 1);
    }

    bool read280(float& t, float& p, float& h) {
      uint8_t d[8];
      if (!i2c_bus::readRegs(addr, 0xF7, d, 8)) return false;
      int32_t adcP = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
      int32_t adcT = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);
      int32_t adcH = ((int32_t)d[6] << 8) | d[7];
      if (adcT == 0x80000 || adcP == 0x80000) return false;
      int32_t var1 = ((((adcT >> 3) - ((int32_t)c280.T1 << 1))) * ((int32_t)c280.T2)) >> 11;
      int32_t var2 = (((((adcT >> 4) - ((int32_t)c280.T1)) * ((adcT >> 4) - ((int32_t)c280.T1))) >> 12) * ((int32_t)c280.T3)) >> 14;
      int32_t t_fine = var1 + var2;
      t = (float)((t_fine * 5 + 128) >> 8) / 100.0f;
      int64_t v1 = (int64_t)t_fine - 128000;
      int64_t v2 = v1 * v1 * (int64_t)c280.P6;
      v2 = v2 + ((v1 * (int64_t)c280.P5) << 17);
      v2 = v2 + (((int64_t)c280.P4) << 35);
      v1 = ((v1 * v1 * (int64_t)c280.P3) >> 8) + ((v1 * (int64_t)c280.P2) << 12);
      v1 = (((((int64_t)1) << 47) + v1)) * ((int64_t)c280.P1) >> 33;
      if (v1 == 0) return false;
      int64_t pp = 1048576 - adcP;
      pp = (((pp << 31) - v2) * 3125) / v1;
      v1 = (((int64_t)c280.P9) * (pp >> 13) * (pp >> 13)) >> 25;
      v2 = (((int64_t)c280.P8) * pp) >> 19;
      pp = ((pp + v1 + v2) >> 8) + (((int64_t)c280.P7) << 4);
      p = (float)pp / 256.0f / 100.0f;                  // Pa -> hPa
      h = 0;
      if (kind == Type::BME280 && adcH != 0x8000) {
        int32_t vx = t_fine - 76800;
        vx = (((((adcH << 14) - (((int32_t)c280.H4) << 20) - (((int32_t)c280.H5) * vx)) + 16384) >> 15) *
              (((((((vx * ((int32_t)c280.H6)) >> 10) * (((vx * ((int32_t)c280.H3)) >> 11) + 32768)) >> 10) + 2097152) * ((int32_t)c280.H2) + 8192) >> 14));
        vx = vx - (((((vx >> 15) * (vx >> 15)) >> 7) * ((int32_t)c280.H1)) >> 4);
        if (vx < 0) vx = 0;
        if (vx > 419430400) vx = 419430400;
        h = (float)(vx >> 12) / 1024.0f;
      }
      return true;
    }

    bool read680(float& t, float& p, float& h) {
      uint8_t d[10];
      if (!i2c_bus::readRegs(addr, 0x1D, d, 10)) return false;
      if (!(d[0] & 0x80)) return false;                 // new_data not set yet
      float adcP = (float)(((uint32_t)d[2] << 12) | ((uint32_t)d[3] << 4) | (d[4] >> 4));
      float adcT = (float)(((uint32_t)d[5] << 12) | ((uint32_t)d[6] << 4) | (d[7] >> 4));
      float adcH = (float)(((uint32_t)d[8] << 8) | d[9]);
      float var1 = ((adcT / 16384.0f) - ((float)c680.T1 / 1024.0f)) * (float)c680.T2;
      float var2 = (((adcT / 131072.0f) - ((float)c680.T1 / 8192.0f)) * ((adcT / 131072.0f) - ((float)c680.T1 / 8192.0f))) * ((float)c680.T3 * 16.0f);
      float t_fine = var1 + var2;
      t = t_fine / 5120.0f;
      var1 = (t_fine / 2.0f) - 64000.0f;
      var2 = var1 * var1 * ((float)c680.P6 / 131072.0f);
      var2 = var2 + (var1 * (float)c680.P5 * 2.0f);
      var2 = (var2 / 4.0f) + ((float)c680.P4 * 65536.0f);
      var1 = ((((float)c680.P3 * var1 * var1) / 16384.0f) + ((float)c680.P2 * var1)) / 524288.0f;
      var1 = (1.0f + (var1 / 32768.0f)) * (float)c680.P1;
      float pc = 1048576.0f - adcP;
      if (var1 == 0) return false;
      pc = ((pc - (var2 / 4096.0f)) * 6250.0f) / var1;
      var1 = ((float)c680.P9 * pc * pc) / 2147483648.0f;
      var2 = pc * ((float)c680.P8 / 32768.0f);
      float var3 = (pc / 256.0f) * (pc / 256.0f) * (pc / 256.0f) * ((float)c680.P10 / 131072.0f);
      pc = pc + (var1 + var2 + var3 + ((float)c680.P7 * 128.0f)) / 16.0f;
      p = pc / 100.0f;
      float tc = t;
      var1 = adcH - (((float)c680.H1 * 16.0f) + (((float)c680.H3 / 2.0f) * tc));
      var2 = var1 * (((float)c680.H2 / 262144.0f) * (1.0f + (((float)c680.H4 / 16384.0f) * tc) + (((float)c680.H5 / 1048576.0f) * tc * tc)));
      var3 = (float)c680.H6 / 16384.0f;
      float var4 = (float)c680.H7 / 2097152.0f;
      h = var2 + ((var3 + (var4 * tc)) * var2 * var2);
      if (h > 100) h = 100; else if (h < 0) h = 0;
      return true;
    }

    float altitude() {
      if (cfg.altitude_m >= 0) return cfg.altitude_m;
      return altHint;                                   // NAN when the weather service has not reported one
    }

    Trend classify(float d, float step, float fast) {
      if (d >= fast) return Trend::RisingFast;
      if (d >= step) return Trend::Rising;
      if (d <= -fast) return Trend::FallingFast;
      if (d <= -step) return Trend::Falling;
      return Trend::Steady;
    }

    // value `back` minutes ago (or the oldest point when the history is shorter); returns the minutes actually spanned
    uint16_t lookBack(uint16_t back, Hist& out) {
      if (!histCount) return 0;
      uint16_t n = back < histCount ? back : histCount;
      uint16_t idx = (uint16_t)((histHead + HIST_MIN - n) % HIST_MIN);
      out = hist[idx];
      return n;
    }

    void updateTrends(Reading& r) {
      Hist old;
      r.span_min = 0;
      uint16_t n = lookBack(cfg.trend_min, old);
      if (n >= 10) {
        r.d_temp = r.temp_c - old.t;
        r.d_hum = r.humidity - old.h;
        r.t_temp = classify(r.d_temp, TEMP_STEP, TEMP_FAST);
        r.t_hum = r.has_humidity ? classify(r.d_hum, HUM_STEP, HUM_FAST) : Trend::Steady;
        r.span_min = n;
      } else { r.d_temp = r.d_hum = 0; r.t_temp = r.t_hum = Trend::Steady; }
      n = lookBack(cfg.pressure_trend_min, old);
      if (n >= 30) {
        // pressure tendency is judged over the whole window; a shorter history is scaled up to it
        r.d_press = (r.pressure_hpa - old.p) * (float)cfg.pressure_trend_min / (float)n;
        r.t_press = classify(r.d_press, PRESS_STEP, PRESS_FAST);
        if (n > r.span_min) r.span_min = n;
      } else { r.d_press = 0; r.t_press = Trend::Steady; }
    }

    void publish(float t, float p, float h, uint32_t now) {
      rawT = t; rawP = p; rawH = h;
      Reading r;
      r.valid = true;
      r.has_humidity = kind != Type::BMP280;
      r.temp_c = t + cfg.temp_offset_c;
      r.humidity = r.has_humidity ? constrain(h + cfg.humidity_offset, 0.0f, 100.0f) : 0;
      r.pressure_hpa = p;
      float alt = altitude();
      if (!isnan(alt)) {
        r.altitude_m = alt;
        r.sea_level_known = true;
        r.sea_level_hpa = p / powf(1.0f - alt / 44330.0f, 5.255f);
      } else { r.sea_level_hpa = p; r.sea_level_known = false; r.altitude_m = 0; }
      r.sample_ms = now;
      if (hist && (lastPoint == 0 || now - lastPoint >= 60000UL)) {
        lastPoint = now;
        hist[histHead] = { r.temp_c, r.humidity, r.pressure_hpa };
        histHead = (uint16_t)((histHead + 1) % HIST_MIN);
        if (histCount < HIST_MIN) histCount++;
      }
      updateTrends(r);
      if (take()) { cur = r; give(); }
    }
  }

  void begin(const IndoorConfig& ic) {
    cfg = ic;
    mtx = xSemaphoreCreateMutex();
    if (!cfg.enabled) { LOGI("indoor: disabled"); return; }
    for (uint8_t a : ADDRS) {
      uint8_t id = 0;
      if (!i2c_bus::readReg(a, REG_ID, id)) continue;
      if (id == 0x60) kind = Type::BME280;
      else if (id == 0x58) kind = Type::BMP280;
      else if (id == 0x61) kind = Type::BME680;
      else continue;
      addr = a;
      break;
    }
    if (kind == Type::None) { LOGI("indoor: no BME280/BMP280/BME680 found at 0x76/0x77"); return; }
    bool ok = kind == Type::BME680 ? init680() : init280();
    if (!ok) { LOGW("indoor: %s at 0x%02X did not answer during setup", typeName(), addr); kind = Type::None; addr = 0; return; }
    hist = (Hist*)heap_caps_malloc(sizeof(Hist) * HIST_MIN, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!hist) hist = (Hist*)malloc(sizeof(Hist) * HIST_MIN);
    LOGI("indoor: %s at 0x%02X", typeName(), addr);
  }

  void apply(const IndoorConfig& ic) {
    bool was = cfg.enabled;
    cfg = ic;
    if (cfg.enabled && !was && kind == Type::None) begin(ic);
    if (!cfg.enabled) { if (take()) { cur = Reading(); give(); } }
  }

  void loop(uint32_t now) {
    if (kind == Type::None || !cfg.enabled) return;
    if (!measuring) {
      uint32_t period = (uint32_t)(cfg.sample_sec ? cfg.sample_sec : 10) * 1000UL;
      if (lastTrigger && now - lastTrigger < period) return;
      lastTrigger = now;
      if (trigger()) { measuring = true; readyAt = now + MEAS_WAIT_MS; }
      else errs++;
      return;
    }
    if ((int32_t)(now - readyAt) < 0) return;
    measuring = false;
    float t, p, h;
    bool ok = kind == Type::BME680 ? read680(t, p, h) : read280(t, p, h);
    if (!ok || t < -45 || t > 90 || p < 300 || p > 1200) { errs++; return; }
    publish(t, p, h, now);
  }

  void setAltitudeHint(float meters) { altHint = meters; }
  Type type() { return kind; }
  const char* typeName() {
    switch (kind) {
      case Type::BMP280: return "BMP280";
      case Type::BME280: return "BME280";
      case Type::BME680: return "BME680";
      default: return "none";
    }
  }
  uint8_t address() { return addr; }
  bool present() { return kind != Type::None && cfg.enabled; }
  Reading reading() { Reading r; if (take()) { r = cur; give(); } return r; }

  size_t history(HistoryPoint* out, size_t max, uint16_t minutes, uint16_t step_min) {
    if (!hist || !histCount || !max) return 0;
    if (!step_min) step_min = 1;
    uint16_t span = minutes < histCount ? minutes : histCount;
    size_t n = 0;
    for (int32_t back = span; back >= 1 && n < max; back -= step_min) {
      uint16_t idx = (uint16_t)((histHead + HIST_MIN - back) % HIST_MIN);
      out[n++] = { (uint16_t)(back - 1), hist[idx].t, hist[idx].h, hist[idx].p };
    }
    return n;
  }

  const char* trendName(Trend t) {
    switch (t) {
      case Trend::RisingFast: return "rising fast";
      case Trend::Rising: return "rising";
      case Trend::Falling: return "falling";
      case Trend::FallingFast: return "falling fast";
      default: return "steady";
    }
  }
  uint32_t errors() { return errs; }
}
