#include "pcf85063.h"
#include "io/i2c_bus.h"
#include "pins.h"
#include "util/timeutil.h"

namespace pcf85063 {
  namespace {
    constexpr uint8_t REG_CTRL1 = 0x00, REG_SEC = 0x04;
    uint8_t bcd2bin(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
    uint8_t bin2bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }
  }

  bool present() {
    uint8_t v;
    return i2c_bus::map().rtc && i2c_bus::readReg(pins::I2C_ADDR_RTC, REG_CTRL1, v);
  }

  bool read(time_t& utc) {
    uint8_t r[7];
    if (!i2c_bus::readRegs(pins::I2C_ADDR_RTC, REG_SEC, r, 7)) return false;
    if (r[0] & 0x80) return false;                 // OS flag: clock integrity not guaranteed
    int sec = bcd2bin(r[0] & 0x7F), min = bcd2bin(r[1] & 0x7F), hour = bcd2bin(r[2] & 0x3F);
    int day = bcd2bin(r[3] & 0x3F), mon = bcd2bin(r[5] & 0x1F), year = 2000 + bcd2bin(r[6]);
    if (mon < 1 || mon > 12 || day < 1 || day > 31 || hour > 23 || min > 59 || sec > 59) return false;
    utc = (time_t)(days_from_civil(year, mon, day) * 86400 + hour * 3600 + min * 60 + sec);
    return utc > 1700000000;
  }

  bool write(time_t utc) {
    struct tm t;
    gmtime_r(&utc, &t);
    uint8_t r[7] = {
      bin2bcd(t.tm_sec), bin2bcd(t.tm_min), bin2bcd(t.tm_hour), bin2bcd(t.tm_mday),
      (uint8_t)t.tm_wday, bin2bcd(t.tm_mon + 1), bin2bcd((t.tm_year + 1900) % 100)
    };
    return i2c_bus::writeRegs(pins::I2C_ADDR_RTC, REG_SEC, r, 7);   // OS flag cleared by writing seconds
  }
}
