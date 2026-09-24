#include "es8311.h"
#include "io/i2c_bus.h"
#include "util/log.h"

namespace es8311 {
  namespace {
    uint8_t addr = 0;
    enum : uint8_t {
      REG_RESET = 0x00, REG_CLK01 = 0x01, REG_CLK02 = 0x02, REG_CLK03 = 0x03, REG_CLK04 = 0x04, REG_CLK05 = 0x05,
      REG_CLK06 = 0x06, REG_CLK07 = 0x07, REG_CLK08 = 0x08, REG_SDP_IN = 0x09, REG_SDP_OUT = 0x0A,
      REG_SYS0D = 0x0D, REG_SYS0E = 0x0E, REG_SYS12 = 0x12, REG_SYS13 = 0x13, REG_ADC1C = 0x1C,
      REG_DAC31 = 0x31, REG_DAC32 = 0x32, REG_DAC37 = 0x37, REG_GP45 = 0x45, REG_CHIP1 = 0xFD, REG_CHIP2 = 0xFE
    };
    bool wr(uint8_t r, uint8_t v) { return i2c_bus::writeReg(addr, r, v); }
    bool upd(uint8_t r, uint8_t mask, uint8_t val) {
      uint8_t v;
      if (!i2c_bus::readReg(addr, r, v)) return false;
      v = (uint8_t)((v & ~mask) | (val & mask));
      return wr(r, v);
    }
  }

  bool probe(uint8_t a) {
    uint8_t id1 = 0, id2 = 0;
    return i2c_bus::readReg(a, REG_CHIP1, id1) && i2c_bus::readReg(a, REG_CHIP2, id2) && id1 == 0x83 && id2 == 0x11;
  }

  bool init(uint8_t a, uint32_t fs) {
    addr = a;
    (void)fs;   // the coefficient row below is identical for 16k/22.05k/44.1k/48k at MCLK = 256 x fs
    bool ok = true;
    ok &= wr(REG_RESET, 0x1F); delay(20);
    ok &= wr(REG_RESET, 0x00);
    ok &= wr(REG_RESET, 0x80);
    ok &= wr(REG_CLK01, 0x3F);             // all clocks on, MCLK from the MCLK pin, not inverted
    ok &= upd(REG_CLK06, 0x20, 0x00);      // BCLK not inverted
    ok &= upd(REG_CLK02, 0xF8, 0x00);      // pre_div 1, pre_multi x1
    ok &= wr(REG_CLK05, 0x00);             // adc_div 1, dac_div 1
    ok &= upd(REG_CLK03, 0x7F, 0x10);      // fs_mode 0, adc_osr 0x10
    ok &= upd(REG_CLK04, 0x7F, 0x10);      // dac_osr 0x10
    ok &= upd(REG_CLK07, 0x3F, 0x00);      // lrck_h
    ok &= wr(REG_CLK08, 0xFF);             // lrck_l  (lrck divider 256)
    ok &= upd(REG_CLK06, 0x1F, 0x03);      // bclk_div 4 (-1)
    ok &= upd(REG_RESET, 0x40, 0x00);      // I2S slave
    ok &= upd(REG_SDP_IN, 0x1C, 0x0C);     // 16-bit I2S
    ok &= upd(REG_SDP_OUT, 0x1C, 0x0C);
    ok &= wr(REG_SYS0D, 0x01);             // power up analog circuitry
    ok &= wr(REG_SYS0E, 0x02);             // enable analog PGA / ADC modulator
    ok &= wr(REG_SYS12, 0x00);             // power up DAC
    ok &= wr(REG_SYS13, 0x10);             // enable output to HP drive
    ok &= wr(REG_ADC1C, 0x6A);             // ADC equalizer bypass, DC offset cancel
    ok &= wr(REG_DAC37, 0x08);             // DAC equalizer bypass
    ok &= wr(REG_GP45, 0x00);
    if (!ok) LOGE("es8311: init failed at 0x%02X", addr);
    else LOGI("es8311: initialised at 0x%02X", addr);
    return ok;
  }

  // DAC volume register: 0.5 dB per step, 0xBF = 0 dB (full scale), values above add digital gain up to +32 dB and
  // clip badly. 100 % therefore maps to 0 dB and each percent below takes 0.5 dB off (60 % = -20 dB); 0 mutes.
  bool setVolume(uint8_t percent) {
    if (percent > 100) percent = 100;
    uint8_t v = percent == 0 ? 0 : (uint8_t)(0xBF - (100 - percent));
    return wr(REG_DAC32, v);
  }

  bool mute(bool on) { return upd(REG_DAC31, 0x60, on ? 0x60 : 0x00); }
}
