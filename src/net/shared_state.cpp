#include "shared_state.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace shared {
  namespace {
    SemaphoreHandle_t mtx = nullptr;
    WeatherData weather;
    NetStatus net;
    bool take() { return mtx && xSemaphoreTake(mtx, pdMS_TO_TICKS(100)) == pdTRUE; }
    void give() { xSemaphoreGive(mtx); }
  }

  void begin() { mtx = xSemaphoreCreateMutex(); }

  void setWeather(const WeatherData& w) { if (take()) { weather = w; give(); } }
  bool getWeather(WeatherData& out) { if (take()) { out = weather; give(); } return out.valid; }
  void getNet(NetStatus& out) { if (take()) { out = net; give(); } }
  void updateNet(void (*fn)(NetStatus&, void*), void* arg) { if (take()) { fn(net, arg); give(); } }
}
