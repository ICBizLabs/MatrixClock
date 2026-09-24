#include "audio_out.h"
#include <ESP_I2S.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <math.h>
#include "es8311.h"
#include "pins.h"
#include "time/time_service.h"
#include "util/timeutil.h"
#include "util/log.h"

namespace audio_out {
  namespace {
    constexpr uint32_t RATE = 22050;
    constexpr size_t FRAMES = 256;
    I2SClass i2s;
    AudioConfig cfg;
    QueueHandle_t q = nullptr;
    bool ready = false;
    int16_t sine[256];
    int16_t buf[FRAMES * 2];
    const char* suppressReason = "";

    void writeSilence(uint32_t ms) {
      memset(buf, 0, sizeof(buf));
      uint32_t frames = RATE * ms / 1000;
      while (frames) {
        size_t n = frames > FRAMES ? FRAMES : frames;
        i2s.write((uint8_t*)buf, n * 4);
        frames -= n;
      }
    }

    // Plays a tone with a 5 ms attack/release envelope. Sweeps from f0 to f1 over the duration.
    void tone(float f0, float f1, uint32_t ms, float amp) {
      uint32_t total = RATE * ms / 1000, done = 0;
      uint32_t phase = 0;                       // 32-bit phase accumulator; the top 8 bits index the sine table
      const uint32_t env = RATE * 5 / 1000;     // envelope length in frames
      while (done < total) {
        size_t n = (total - done) > FRAMES ? FRAMES : (total - done);
        for (size_t i = 0; i < n; i++) {
          uint32_t k = done + i;
          float f = f0 + (f1 - f0) * (float)k / (float)total;
          uint32_t inc = (uint32_t)(f * 4294967296.0f / RATE);       // one full table cycle = 2^32
          phase += inc;
          float e = 1.0f;
          if (k < env) e = (float)k / env;
          else if (total - k < env) e = (float)(total - k) / env;
          int16_t s = (int16_t)(sine[(phase >> 24) & 0xFF] * amp * e);
          buf[2 * i] = s;
          buf[2 * i + 1] = s;
        }
        i2s.write((uint8_t*)buf, n * 4);
        done += n;
      }
    }

    void play(ChimeStyle style) {
      const float A = 0.6f;
      switch (style) {
        case ChimeStyle::TwoTone:
          for (int r = 0; r < 2; r++) { tone(880, 880, 150, A); writeSilence(40); tone(1175, 1175, 150, A); writeSilence(120); }
          break;
        case ChimeStyle::TripleBeep:
          for (int r = 0; r < 3; r++) { tone(1000, 1000, 80, A); writeSilence(70); }
          break;
        case ChimeStyle::Chirp:
          tone(600, 1400, 300, A); writeSilence(60); tone(600, 1400, 300, A);
          break;
        default: break;
      }
    }

    void task(void*) {
      ChimeStyle style;
      for (;;) {
        if (xQueueReceive(q, &style, portMAX_DELAY) != pdTRUE) continue;
        es8311::mute(false);
        digitalWrite(pins::PA_EN, HIGH);
        delay(20);
        writeSilence(200);
        play(style);
        writeSilence(100);
        digitalWrite(pins::PA_EN, LOW);
        es8311::mute(!cfg.enabled);
      }
    }
  }

  bool begin(const AudioConfig& ac, uint8_t codecAddr) {
    cfg = ac;
    pinMode(pins::PA_EN, OUTPUT);
    digitalWrite(pins::PA_EN, LOW);
    if (!codecAddr) { LOGW("audio: no ES8311 found, audio disabled"); return false; }
    for (int i = 0; i < 256; i++) sine[i] = (int16_t)(sinf(2.0f * (float)M_PI * i / 256.0f) * 32000.0f);
    i2s.setPins(pins::I2S_BCLK, pins::I2S_LRCK, pins::I2S_DOUT, pins::I2S_DIN, pins::I2S_MCLK);
    if (!i2s.begin(I2S_MODE_STD, RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) { LOGE("audio: I2S begin failed"); return false; }
    if (!es8311::init(codecAddr, RATE)) return false;
    es8311::setVolume(cfg.volume);
    es8311::mute(true);
    q = xQueueCreate(4, sizeof(ChimeStyle));
    xTaskCreatePinnedToCore(task, "audio", 4096, nullptr, 3, nullptr, 0);
    ready = true;
    LOGI("audio: ready (volume %u%%)", cfg.volume);
    return true;
  }

  void apply(const AudioConfig& ac) {
    cfg = ac;
    if (ready) es8311::setVolume(cfg.volume);
  }

  bool available() { return ready; }

  bool inQuietHours() {
    struct tm lt;
    if (!cfg.quiet.enabled || !timesvc::localNow(lt)) return false;
    return in_window(cfg.quiet.start, cfg.quiet.end, (uint16_t)(lt.tm_hour * 60 + lt.tm_min));
  }

  bool chime(ChimeStyle style, bool force) {
    suppressReason = "";
    if (!ready) { suppressReason = "no audio"; return false; }
    if (style == ChimeStyle::None) { suppressReason = "chime style none"; return false; }
    if (!force && !cfg.enabled) { suppressReason = "audio disabled"; LOGI("chime suppressed: audio disabled"); return false; }
    if (!force && inQuietHours()) { suppressReason = "quiet hours"; LOGI("chime suppressed: quiet hours"); return false; }
    return xQueueSend(q, &style, 0) == pdTRUE;
  }

  const char* lastSuppressReason() { return suppressReason; }
}
