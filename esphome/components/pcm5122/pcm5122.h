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
static const uint8_t PCM5122_RESET_MODULES = 0x10;     // RSTM: reset audio modules
static const uint8_t PCM5122_AUDIO_FORMAT_I2S = 0x00;  // AFMT = I2S (bits [5:4] = 00)
// ALEN (word length) occupies bits [1:0] of the audio format register
static const uint8_t PCM5122_AUDIO_FORMAT_ALEN_16BIT = 0x00;
static const uint8_t PCM5122_AUDIO_FORMAT_ALEN_24BIT = 0x02;
static const uint8_t PCM5122_AUDIO_FORMAT_ALEN_32BIT = 0x03;
static const uint8_t PCM5122_ERROR_DETECT_IGNORE_CLKHALT = (1 << 3);
static const uint8_t PCM5122_ERROR_DETECT_DISABLE_DIV_AUTOSET = (1 << 1);
static const uint8_t PCM5122_PLL_REF_MASK = (7 << 4);        // SREF bits [6:4]
static const uint8_t PCM5122_PLL_REF_SOURCE_BCK = (1 << 4);  // SREF = 001 (BCK)

enum PCM5122BitsPerSample : uint8_t {
  PCM5122_BITS_PER_SAMPLE_16 = 16,
  PCM5122_BITS_PER_SAMPLE_24 = 24,
  PCM5122_BITS_PER_SAMPLE_32 = 32,
};

class PCM5122 final : public audio_dac::AudioDac, public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }

  void set_bits_per_sample(PCM5122BitsPerSample bits_per_sample) { this->bits_per_sample_ = bits_per_sample; }

  bool set_mute_off() override;
  bool set_mute_on() override;
  bool set_volume(float volume) override;

  bool is_muted() override;
  float volume() override;

  friend class PCM5122GPIOPin;

 protected:
  bool select_page_(uint8_t page);
  bool write_mute_();
  bool write_volume_();

  float volume_{1.0f};        // Matches chip post-reset DVOL default (0x30 = 0 dB)
  int16_t current_page_{-1};  // -1 = unknown; cached to skip redundant page-select writes
  bool is_muted_{false};
  PCM5122BitsPerSample bits_per_sample_{PCM5122_BITS_PER_SAMPLE_16};
};

}  // namespace esphome::pcm5122
