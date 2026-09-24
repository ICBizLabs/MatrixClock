#include "pca9557.h"
#include "io/i2c_bus.h"
#include "util/log.h"

namespace pca9557 {
  namespace {
    uint8_t addr = 0;
    constexpr uint8_t REG_INPUT = 0x00, REG_POLARITY = 0x02, REG_CONFIG = 0x03;
    constexpr uint8_t KEY_MASK = 0x0E;   // IO1 (K1), IO2 (K3), IO3 (K2)
  }

  bool begin(uint8_t a) {
    addr = a;
    if (!addr) return false;
    uint8_t cfg;
    if (!i2c_bus::readReg(addr, REG_CONFIG, cfg)) return false;
    bool ok = i2c_bus::writeReg(addr, REG_POLARITY, 0x00);        // reset default inverts IO4..7; we want raw levels
    ok &= i2c_bus::writeReg(addr, REG_CONFIG, cfg | KEY_MASK);    // keys as inputs
    uint8_t in = 0;
    i2c_bus::readReg(addr, REG_INPUT, in);
    LOGI("pca9557: at 0x%02X config 0x%02X inputs 0x%02X", addr, cfg | KEY_MASK, in);
    return ok;
  }

  bool readInputs(uint8_t& v) { return addr && i2c_bus::readReg(addr, REG_INPUT, v); }
}
