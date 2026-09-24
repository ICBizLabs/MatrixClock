#pragma once
#include <Arduino.h>

// Copy of the last rendered frame (RGB565) for the live preview in the web UI. Written by the render loop,
// read by the web server task in chunks.
namespace frame_snapshot {
  void begin(uint16_t w, uint16_t h);
  void update(const uint16_t* pixels);                       // w*h pixels
  size_t bytes();                                            // total size of one frame in bytes
  size_t copy(uint8_t* out, size_t offset, size_t maxLen);   // returns bytes copied
  uint16_t width();
  uint16_t height();
}
