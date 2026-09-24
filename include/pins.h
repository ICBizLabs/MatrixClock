#pragma once
#include <stdint.h>

// Seengreat RGB Matrix HUB75 S3 (SKU 260612) pin map - https://seengreat.com/wiki/214/rgb-matrix-hub75-s3
namespace pins {
  // HUB75 connector
  constexpr int8_t R1 = 5,  G1 = 4,  B1 = 6;
  constexpr int8_t R2 = 15, G2 = 7,  B2 = 17;
  constexpr int8_t A = 8, B = 18, C = 10, D = 9, E = 16;
  constexpr int8_t LAT = 11, OE = 13, CLK = 12;

  // Audio: ES8311 codec (DAC) + ES7210 (mics) on I2S, NS4150 speaker amp enable
  constexpr int8_t I2S_MCLK = 38, I2S_BCLK = 48, I2S_LRCK = 21;
  constexpr int8_t I2S_DOUT = 14;   // ESP -> codec (ES8311 DSDIN)
  constexpr int8_t I2S_DIN  = 47;   // ES7210 SDOUT -> ESP
  constexpr int8_t PA_EN    = 3;    // speaker amplifier enable, active high

  // I2C: PCF85063 RTC (0x51), ES8311 (0x18/0x19), PCA9557 IO expander (0x18-0x1F, probed)
  constexpr int8_t SDA = 1, SCL = 2;
  constexpr uint8_t I2C_ADDR_RTC = 0x51;

  // micro-SD (SPI), unused by the firmware for now
  constexpr int8_t SD_MISO = 42, SD_CLK = 41, SD_MOSI = 40, SD_CS = 39;

  constexpr int8_t BOOT_BTN = 0;
}
