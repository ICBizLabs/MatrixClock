#pragma once
#include <Arduino.h>

// Shared I2C bus (RTC, ES8311 codec, PCA9557 expander). All access goes through one mutex because the
// RTC is written from the network task, the codec from the audio task and the expander from loop().
namespace i2c_bus {
  struct Map {
    uint8_t es8311 = 0;    // 0 = not found
    uint8_t pca9557 = 0;
    uint8_t rtc = 0;
    uint8_t found[16];
    uint8_t n = 0;
  };

  void begin();
  bool lock(uint32_t timeout_ms = 50);
  void unlock();
  const Map& identify();    // scans the bus once and classifies devices; cached afterwards
  void requestRescan();     // scan again from the main loop (hot-plugged sensor); the result lands in map()
  void loop();              // main loop: runs a requested scan on the loop task
  const Map& map();

  // Register helpers (take the lock internally)
  bool writeReg(uint8_t addr, uint8_t reg, uint8_t val);
  bool readReg(uint8_t addr, uint8_t reg, uint8_t& val);
  bool readRegs(uint8_t addr, uint8_t reg, uint8_t* buf, size_t n);
  bool writeRegs(uint8_t addr, uint8_t reg, const uint8_t* buf, size_t n);
}
