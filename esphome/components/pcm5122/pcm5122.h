#pragma once

#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace pcm5122 {

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
static const uint8_t PCM5122_REG_GPIO_OUTPUT_SELECT = 0x50;  // Base address; GPIO n uses offset n-1
static const uint8_t PCM5122_REG_GPIO_OUTPUT = 0x56;
static const uint8_t PCM5122_REG_GPIO_INVERT = 0x57;
static const uint8_t PCM5122_REG_GPIO_INPUT = 0x77;

class PCM5122 : public audio_dac::AudioDac, public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  bool set_mute_off() override;
  bool set_mute_on() override;
  bool set_volume(float volume) override;

  bool is_muted() override;
  float volume() override;

 protected:
  bool write_mute_();
  bool write_volume_();

  float volume_{0.0f};
};

}  // namespace pcm5122
}  // namespace esphome
