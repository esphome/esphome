#pragma once

#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::pcm5122 {

// Page 0 register addresses
static const uint8_t PCM5122_REG_PAGE_SELECT = 0x00;
static const uint8_t PCM5122_REG_RESET = 0x01;
static const uint8_t PCM5122_REG_MUTE = 0x03;
static const uint8_t PCM5122_REG_GPIO_ENABLE = 0x08;
static const uint8_t PCM5122_REG_PLL_REF = 0x0D;
static const uint8_t PCM5122_REG_ERROR_DETECT = 0x25;
static const uint8_t PCM5122_REG_AUDIO_FORMAT = 0x28;
static const uint8_t PCM5122_REG_DVOL_LEFT = 0x3D;
static const uint8_t PCM5122_REG_DVOL_RIGHT = 0x3E;
static const uint8_t PCM5122_REG_GPIO_OUTPUT_SELECT = 0x50;       // Base address; GPIO n uses offset n-1
static const uint8_t PCM5122_GPIO_OUTPUT_SELECT_REGISTER = 0x02;  // GPIO driven by GPIO_OUTPUT register (reg 0x56)
static const uint8_t PCM5122_REG_GPIO_OUTPUT = 0x56;
static const uint8_t PCM5122_REG_GPIO_INVERT = 0x57;
static const uint8_t PCM5122_REG_GPIO_INPUT = 0x77;

// Register values for init sequence
static const uint8_t PCM5122_RESET_MODULES = 0x10;           // RSTM: reset audio modules
static const uint8_t PCM5122_AUDIO_FORMAT_I2S_32BIT = 0x03;  // AFMT=I2S, ALEN=32-bit
static const uint8_t PCM5122_ERROR_DETECT_IGNORE_CLKHALT = (1 << 3);
static const uint8_t PCM5122_ERROR_DETECT_DISABLE_DIV_AUTOSET = (1 << 1);
static const uint8_t PCM5122_PLL_REF_MASK = (7 << 4);        // SREF bits [6:4]
static const uint8_t PCM5122_PLL_REF_SOURCE_BCK = (1 << 4);  // SREF = 001 (BCK)

class PCM5122 : public audio_dac::AudioDac, public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }

  bool set_mute_off() override;
  bool set_mute_on() override;
  bool set_volume(float volume) override;

  bool is_muted() override;
  float volume() override;

  bool select_page(uint8_t page) { return this->write_byte(PCM5122_REG_PAGE_SELECT, page); }

 protected:
  bool write_mute_();
  bool write_volume_();

  bool is_muted_{false};
  float volume_{1.0f};  // Matches chip post-reset DVOL default (0x30 = 0 dB)
};

}  // namespace esphome::pcm5122
