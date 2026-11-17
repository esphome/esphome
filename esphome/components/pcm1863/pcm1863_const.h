#pragma once

#include <cstdint>

namespace esphome::pcm1863 {

// PCM1863 page 0 register addresses.
// Names and layout follow the Texas Instruments PCM186x family register map (SLAS831D).
static constexpr uint8_t PCM1863_REG_PAGE = 0x00;          /* Page selector; writing 0xFE triggers a full reset */
static constexpr uint8_t PCM1863_REG_PGA_VAL_CH1_L = 0x01; /* ADC1 left PGA gain, 0.5 dB/LSB, two's complement */
static constexpr uint8_t PCM1863_REG_PGA_VAL_CH1_R = 0x02; /* ADC1 right PGA gain, 0.5 dB/LSB, two's complement */
static constexpr uint8_t PCM1863_REG_ADC1_INPUT_SEL_L = 0x06;
static constexpr uint8_t PCM1863_REG_ADC1_INPUT_SEL_R = 0x07;
static constexpr uint8_t PCM1863_REG_PCM_CFG = 0x0B;    /* Word length and serial audio format */
static constexpr uint8_t PCM1863_REG_POWER_CTRL = 0x70; /* Power down, sleep and standby */

// PCM1863_REG_PAGE: the register holds the page number, and the two documented
// commands are selecting a page and resetting every register
static constexpr uint8_t PCM1863_PAGE_0 = 0x00;
static constexpr uint8_t PCM1863_RESET = 0xFE;

// PCM1863_REG_PCM_CFG: receive word length in bits 7:6, transmit word length in bits 3:2,
// serial audio format in bits 1:0. Bit 5 is reserved and bit 4 only applies to TDM; both
// are zero at reset and are written as zero here.
static constexpr uint8_t PCM1863_PCM_CFG_RX_WLEN_SHIFT = 6;
static constexpr uint8_t PCM1863_PCM_CFG_TX_WLEN_SHIFT = 2;

static constexpr uint8_t PCM1863_PCM_CFG_WLEN_32 = 0x00;
static constexpr uint8_t PCM1863_PCM_CFG_WLEN_24 = 0x01;
static constexpr uint8_t PCM1863_PCM_CFG_WLEN_20 = 0x02;
static constexpr uint8_t PCM1863_PCM_CFG_WLEN_16 = 0x03;

static constexpr uint8_t PCM1863_PCM_CFG_FMT_I2S = 0x00;

// PCM1863_REG_ADC1_INPUT_SEL_L / _R
// Bit 7 selects signal polarity, bit 6 is reserved and must always be written as 1,
// and bits 5:0 pick the analog input. See the PCM186x datasheet, "MUX, MIX, and
// Polarity Input Selection".
static constexpr uint8_t PCM1863_INPUT_SEL_RSV = 0x40;  // bit 6, always write 1
// Bits 5:0 select one input: none, VIN1 to VIN4 single-ended (0x01 to 0x08), then the
// differential pairs VIN1/VIN2 (0x10) and VIN4/VIN3 (0x20)

// PCM1863_REG_POWER_CTRL
// Bits 2:0 are power down, sleep and standby; all three are clear in the run state.
// Bits 7:3 are reserved and the datasheet requires them to always be written as 0b01110,
// which is also their reset value. Writing them as zero stops the analog section.
static constexpr uint8_t PCM1863_PWR_CTRL_RSV = 0x70;

// The PGA gain registers hold a two's complement value in 0.5 dB steps,
// from -24 (-12 dB) to +80 (+40 dB)
static constexpr float PCM1863_MIC_GAIN_MIN = -12.0f;
static constexpr float PCM1863_MIC_GAIN_MAX = 40.0f;
static constexpr float PCM1863_MIC_GAIN_STEP = 0.5f;

}  // namespace esphome::pcm1863
