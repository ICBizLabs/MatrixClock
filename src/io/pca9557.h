#pragma once
#include <Arduino.h>

// PCA9557 8-bit I/O expander driving the board's 3-way thumb-wheel switch.
namespace pca9557 {
  bool begin(uint8_t addr);          // configures IO1..IO3 as inputs, leaves the rest as found
  bool readInputs(uint8_t& v);
}
