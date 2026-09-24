#pragma once
#include <Arduino.h>

// Thumb-wheel keys (through the PCA9557): short and long presses, debounced. Polled from loop().
namespace buttons {
  enum Key : uint8_t { K1 = 0, K2 = 1, K3 = 2 };
  typedef void (*Handler)(uint8_t key, bool longPress);
  void begin(Handler h);
  void loop();
  bool available();
}
