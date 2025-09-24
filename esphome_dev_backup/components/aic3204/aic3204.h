#pragma once

#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace aic3204 {

// TLV320AIC3204 Register Addresses
// Page 0
static const uint8_t AIC3204_PAGE_CTRL = 0x00;     // Register 0  - Page Control
static const uint8_t AIC3204_SW_RST = 0x01;        // Register 1  - Software Reset
static const uint8_t AIC3204_CLK_PLL1 = 0x04;      // Register 4  - Clock Setting Register 1, Multiplexers
static const uint8_t AIC3204_CLK_PLL2 = 0x05;      // Register 5  - Clock Setting Register 2, P and R values
static const uint8_t AIC3204_CLK_PLL3 = 0x06;      // Register 6  - Clock Setting Register 3, J values
static const uint8_t AIC3204_NDAC = 0x0B;          // Register 11 - NDAC Divider Value
static const uint8_t AIC3204_MDAC = 0x0C;          // Register 12 - MDAC Divider Value
static const uint8_t AIC3204_DOSR = 0x0E;          // Register 14 - DOSR Divider Value (LS Byte)
static const uint8_t AIC3204_NADC = 0x12;          // Register 18 - NADC Divider Value
static const uint8_t AIC3204_MADC = 0x13;          // Register 19 - MADC Divider Value
static const uint8_t AIC3204_AOSR = 0x14;          // Register 20 - AOSR Divider Value
static const uint8_t AIC3204_CODEC_IF = 0x1B;      // Register 27 - CODEC Interface Control
static const uint8_t AIC3204_AUDIO_IF_4 = 0x1F;    // Register 31 - Audio Interface Setting Register 4
static const uint8_t AIC3204_AUDIO_IF_5 = 0x20;    // Register 32 - Audio Interface Setting Register 5
static const uint8_t AIC3204_SCLK_MFP3 = 0x38;     // Register 56 - SCLK/MFP3 Function Control
static const uint8_t AIC3204_DAC_SIG_PROC = 0x3C;  // Register 60 - DAC Sig Processing Block Control
static const uint8_t AIC3204_ADC_SIG_PROC = 0x3D;  // Register 61 - ADC Sig Processing Block Control
static const uint8_t AIC3204_DAC_CH_SET1 = 0x3F;   // Register 63 - DAC Channel Setup 1
static const uint8_t AIC3204_DAC_CH_SET2 = 0x40;   // Register 64 - DAC Channel Setup 2
static const uint8_t AIC3204_DACL_VOL_D = 0x41;    // Register 65 - DAC Left Digital Vol Control
static const uint8_t AIC3204_DACR_VOL_D = 0x42;    // Register 66 - DAC Right Digital Vol Control
static const uint8_t AIC3204_DRC_ENABLE = 0x44;
static const uint8_t AIC3204_ADC_CH_SET = 0x51;    // Register 81 - ADC Channel Setup
static const uint8_t AIC3204_ADC_FGA_MUTE = 0x52;  // Register 82 - ADC Fine Gain Adjust/Mute

// Page 1
static const uint8_t AIC3204_PWR_CFG = 0x01;       // Register 1  - Power Config
static const uint8_t AIC3204_LDO_CTRL = 0x02;      // Register 2  - LDO Control
static const uint8_t AIC3204_PLAY_CFG1 = 0x03;     // Register 3  - Playback Config 1
static const uint8_t AIC3204_PLAY_CFG2 = 0x04;     // Register 4  - Playback Config 2
static const uint8_t AIC3204_OP_PWR_CTRL = 0x09;   // Register 9  - Output Driver Power Control
static const uint8_t AIC3204_CM_CTRL = 0x0A;       // Register 10 - Common Mode Control
static const uint8_t AIC3204_HPL_ROUTE = 0x0C;     // Register 12 - HPL Routing Select
static const uint8_t AIC3204_HPR_ROUTE = 0x0D;     // Register 13 - HPR Routing Select
static const uint8_t AIC3204_LOL_ROUTE = 0x0E;     // Register 14 - LOL Routing Selection
static const uint8_t AIC3204_LOR_ROUTE = 0x0F;     // Register 15 - LOR Routing Selection
static const uint8_t AIC3204_HPL_GAIN = 0x10;      // Register 16 - HPL Driver Gain
static const uint8_t AIC3204_HPR_GAIN = 0x11;      // Register 17 - HPR Driver Gain
static const uint8_t AIC3204_LOL_DRV_GAIN = 0x12;  // Register 18 - LOL Driver Gain Setting
static const uint8_t AIC3204_LOR_DRV_GAIN = 0x13;  // Register 19 - LOR Driver Gain Setting
static const uint8_t AIC3204_HP_START = 0x14;      // Register 20 - Headphone Driver Startup
static const uint8_t AIC3204_LPGA_P_ROUTE = 0x34;  // Register 52 - Left PGA Positive Input Route
static const uint8_t AIC3204_LPGA_N_ROUTE = 0x36;  // Register 54 - Left PGA Negative Input Route
static const uint8_t AIC3204_RPGA_P_ROUTE = 0x37;  // Register 55 - Right PGA Positive Input Route
static const uint8_t AIC3204_RPGA_N_ROUTE = 0x39;  // Register 57 - Right PGA Negative Input Route
static const uint8_t AIC3204_LPGA_VOL = 0x3B;      // Register 59 - Left PGA Volume
static const uint8_t AIC3204_RPGA_VOL = 0x3C;      // Register 60 - Right PGA Volume
static const uint8_t AIC3204_ADC_PTM = 0x3D;       // Register 61 - ADC Power Tune Config
static const uint8_t AIC3204_AN_IN_CHRG = 0x47;    // Register 71 - Analog Input Quick Charging Config
static const uint8_t AIC3204_REF_STARTUP = 0x7B;   // Register 123 - Reference Power Up Config

class AIC3204 : public audio_dac::AudioDac, public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  bool set_mute_off() override;
  bool set_mute_on() override;
  bool set_auto_mute_mode(uint8_t auto_mute_mode);
  bool set_volume(float volume) override;

  bool is_muted() override;
  float volume() override;

 protected:
  bool write_mute_();
  bool write_volume_();

  uint8_t auto_mute_mode_{0};
  float volume_{0};
};

}  // namespace aic3204
}  // namespace esphome
