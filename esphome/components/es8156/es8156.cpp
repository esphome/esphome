#include "es8156.h"
#include "es8156_const.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace es8156 {

static const char *const TAG = "es8156";

// Mark the component as failed; use only in setup
#define ES8156_ERROR_FAILED(func) \
  if (!(func)) { \
    this->mark_failed(); \
    return; \
  }

void ES8156::setup() {
  // REG02 MODE CONFIG 1: Enable software mode for I2C control of volume/mute
  // Bit 2: SOFT_MODE_SEL=1 (software mode enabled)
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG02_SCLK_MODE, 0x04));

  // Analog system configuration (active-low power down bits, active-high enables)
  // REG20 ANALOG SYSTEM: Configure analog signal path
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG20_ANALOG_SYS1, 0x2A));

  // REG21 ANALOG SYSTEM: VSEL=0x1C (bias level ~120%), normal VREF ramp speed
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG21_ANALOG_SYS2, 0x3C));

  // REG22 ANALOG SYSTEM: Line out mode (HPSW=0), OUT_MUTE=0 (not muted)
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG22_ANALOG_SYS3, 0x00));

  // REG24 ANALOG SYSTEM: Low power mode for VREFBUF, HPCOM, DACVRP; DAC normal power
  // Bits 2:0 = 0x07: LPVREFBUF=1, LPHPCOM=1, LPDACVRP=1, LPDAC=0
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG24_ANALOG_LP, 0x07));

  // REG23 ANALOG SYSTEM: Lowest bias (IBIAS_SW=0), VMIDLVL=VDDA/2, normal impedance
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG23_ANALOG_SYS4, 0x00));

  // Timing and interface configuration
  // REG0A/0B TIME CONTROL: Fast state machine transitions
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG0A_TIME_CONTROL1, 0x01));
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG0B_TIME_CONTROL2, 0x01));

  // REG11 SDP INTERFACE CONFIG: Default I2S format (24-bit, I2S mode)
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG11_DAC_SDP, 0x00));

  // REG19 EQ CONTROL 1: EQ disabled (EQ_ON=0), EQ_BAND_NUM=2
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG19_EQ_CONTROL1, 0x20));

  // REG0D P2S CONTROL: Parallel-to-serial converter settings
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG0D_P2S_CONTROL, 0x14));

  // REG09 MISC CONTROL 2: Default settings
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG09_MISC_CONTROL2, 0x00));

  // REG18 MISC CONTROL 3: Stereo channel routing, no inversion
  // Bits 5:4 CHN_CROSS: 0=L→L/R→R, 1=L to both, 2=R to both, 3=swap L/R
  // Bits 3:2: LCH_INV/RCH_INV channel inversion
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG18_MISC_CONTROL3, 0x00));

  // REG08 CLOCK OFF: Enable all internal clocks (0x3F = all clock gates open)
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG08_CLOCK_ON_OFF, 0x3F));

  // REG00 RESET CONTROL: Reset sequence
  // First: RST_DIG=1 (assert digital reset)
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG00_RESET, 0x02));
  // Then: CSM_ON=1 (enable chip state machine), RST_DIG=1
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG00_RESET, 0x03));

  // REG25 ANALOG SYSTEM: Power up analog blocks
  // VMIDSEL=2 (normal VMID operation), PDN_ANA=0, ENREFR=0, ENHPCOM=0
  // PDN_DACVREFGEN=0, PDN_VREFBUF=0, PDN_DAC=0 (all enabled)
  ES8156_ERROR_FAILED(this->write_byte(ES8156_REG25_ANALOG_SYS5, 0x20));
}

void ES8156::dump_config() {
  ESP_LOGCONFIG(TAG, "ES8156 Audio Codec:");

  if (this->is_failed()) {
    ESP_LOGCONFIG(TAG, "  Failed to initialize");
    return;
  }
}

bool ES8156::set_volume(float volume) {
  volume = clamp(volume, 0.0f, 1.0f);
  uint8_t reg = remap<uint8_t, float>(volume, 0.0f, 1.0f, 0, 255);
  ESP_LOGV(TAG, "Setting ES8156_REG14_VOLUME_CONTROL to %u (volume: %f)", reg, volume);
  return this->write_byte(ES8156_REG14_VOLUME_CONTROL, reg);
}

float ES8156::volume() {
  uint8_t reg;
  this->read_byte(ES8156_REG14_VOLUME_CONTROL, &reg);
  return remap<float, uint8_t>(reg, 0, 255, 0.0f, 1.0f);
}

bool ES8156::set_mute_state_(bool mute_state) {
  uint8_t reg13;

  this->is_muted_ = mute_state;

  if (!this->read_byte(ES8156_REG13_DAC_MUTE, &reg13)) {
    return false;
  }

  ESP_LOGV(TAG, "Read ES8156_REG13_DAC_MUTE: %u", reg13);

  if (mute_state) {
    reg13 |= BIT(1) | BIT(2);
  } else {
    reg13 &= ~(BIT(1) | BIT(2));
  }

  ESP_LOGV(TAG, "Setting ES8156_REG13_DAC_MUTE to %u (muted: %s)", reg13, YESNO(mute_state));
  return this->write_byte(ES8156_REG13_DAC_MUTE, reg13);
}

}  // namespace es8156
}  // namespace esphome
