#pragma once
#include <ArduinoJson.h>
#include <esp_heap_caps.h>

// ArduinoJson allocator that prefers PSRAM (falls back to internal RAM when PSRAM is missing or full).
struct SpiRamAllocator : ArduinoJson::Allocator {
  void* allocate(size_t n) override {
    void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : heap_caps_malloc(n, MALLOC_CAP_8BIT);
  }
  void deallocate(void* p) override { heap_caps_free(p); }
  void* reallocate(void* p, size_t n) override {
    void* q = heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return q ? q : heap_caps_realloc(p, n, MALLOC_CAP_8BIT);
  }
};

inline SpiRamAllocator* psramAllocator() {
  static SpiRamAllocator alloc;
  return &alloc;
}
