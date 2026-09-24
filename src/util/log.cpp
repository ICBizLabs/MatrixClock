#include "log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_heap_caps.h>

namespace {
  constexpr size_t RING_SIZE = 4096;
  char*  ring = nullptr;
  size_t head = 0;          // next write position
  bool   wrapped = false;
  SemaphoreHandle_t mtx = nullptr;

  void ringPut(const char* s, size_t n) {
    if (!ring) return;
    for (size_t i = 0; i < n; i++) {
      ring[head] = s[i];
      head = (head + 1) % RING_SIZE;
      if (head == 0) wrapped = true;
    }
  }
}

void log_begin() {
  mtx = xSemaphoreCreateMutex();
  ring = (char*)heap_caps_malloc(RING_SIZE, MALLOC_CAP_SPIRAM);
  if (!ring) ring = (char*)malloc(RING_SIZE);
}

void log_printf(char level, const char* fmt, ...) {
  char line[256];
  int n = snprintf(line, sizeof(line), "[%8lu] %c ", (unsigned long)millis(), level);
  va_list ap;
  va_start(ap, fmt);
  n += vsnprintf(line + n, sizeof(line) - n - 1, fmt, ap);
  va_end(ap);
  if (n > (int)sizeof(line) - 2) n = sizeof(line) - 2;
  line[n++] = '\n';
  line[n] = '\0';
  bool locked = mtx && xSemaphoreTake(mtx, pdMS_TO_TICKS(20)) == pdTRUE;
  Serial.write((const uint8_t*)line, n);
  ringPut(line, n);
  if (locked) xSemaphoreGive(mtx);
}

String log_dump() {
  String out;
  if (!ring) return out;
  bool locked = mtx && xSemaphoreTake(mtx, pdMS_TO_TICKS(50)) == pdTRUE;
  if (wrapped) {
    out.reserve(RING_SIZE + 1);
    out.concat(ring + head, RING_SIZE - head);
    out.concat(ring, head);
  } else {
    out.concat(ring, head);
  }
  if (locked) xSemaphoreGive(mtx);
  return out;
}
