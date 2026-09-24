#pragma once
#include <Arduino.h>
#include <time.h>

// Minimal PCF85063 RTC driver (I2C 0x51). Time is stored in UTC. Returns false when the chip is missing or the
// oscillator-stop flag says the time is not trustworthy.
namespace pcf85063 {
  bool present();
  bool read(time_t& utc);
  bool write(time_t utc);
}
