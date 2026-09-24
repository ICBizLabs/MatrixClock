#pragma once
#include <Arduino.h>
#include "config/config.h"
#include "canvas.h"

// HUB75 panel bring-up and frame presentation (ESP32-HUB75-MatrixPanel-DMA).
namespace panel {
  bool begin(const PanelConfig& pc, uint8_t brightness, float gamma);
  bool valid();
  void setBrightness(uint8_t level);       // clamped to PanelConfig::max_brightness
  uint8_t brightness();
  void setGamma(float g);                  // rebuilds the 256-entry LUT
  void setLatchBlanking(uint8_t n);        // live adjustable
  void present(const Canvas& c, bool force = false);   // pushes changed pixels only (or everything when force / double buffer)
  int refreshRateHz();
  const char* driverName();
}
