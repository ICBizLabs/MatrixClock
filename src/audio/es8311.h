#pragma once
#include <Arduino.h>

// ES8311 codec, DAC path only: I2S slave, 16-bit, MCLK = 256 x fs from the MCLK pin.
// Register sequence ported from Espressif's es8311 component (esp-bsp, Apache-2.0).
namespace es8311 {
  bool probe(uint8_t addr);                 // chip ID 0x83 / 0x11
  bool init(uint8_t addr, uint32_t fs);
  bool setVolume(uint8_t percent);          // 0..100
  bool mute(bool on);
}
