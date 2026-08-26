#pragma once

#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"

namespace esphome::pcm5122 {

// Page 0 register addresses
static const uint8_t PCM5122_REG_PAGE_SELECT = 0x00;
static const uint8_t PCM5122_REG_RESET = 0x01;
static const uint8_t PCM5122_REG_POWER_CONTROL = 0x02;
static const uint8_t PCM5122_REG_MUTE = 0x03;
static const uint8_t PCM5122_REG_GPIO_ENABLE = 0x08;
static const uint8_t PCM5122_REG_PLL_REF = 0x0D;
static const uint8_t PCM5122_REG_ERROR_DETECT = 0x25;
static const uint8_t PCM5122_REG_AUDIO_FORMAT = 0x28;
static const uint8_t PCM5122_REG_DAC_DATA_PATH = 0x2A;
static const uint8_t PCM5122_REG_DVOL_LEFT = 0x3D;
static const uint8_t PCM5122_REG_DVOL_RIGHT = 0x3E;
static const uint8_t PCM5122_REG_GPIO_OUTPUT_SELECT = 0x50;       // Base address; GPIO n uses offset n-1
static const uint8_t PCM5122_GPIO_OUTPUT_SELECT_REGISTER = 0x02;  // GPIO driven by GPIO_OUTPUT register (reg 0x56)
static const uint8_t PCM5122_REG_GPIO_OUTPUT = 0x56;
static const uint8_t PCM5122_REG_GPIO_INVERT = 0x57;
static const uint8_t PCM5122_REG_GPIO_INPUT = 0x77;

// Page 1 register addresses
static const uint8_t PCM5122_REG_ANALOG_GAIN = 0x02;

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

// Page 0, Register 2 (Power Control): RQST = standby request, RQPD = powerdown request (§10.5.3)
static const uint8_t PCM5122_POWER_CONTROL_RQST = (1 << 4);
static const uint8_t PCM5122_POWER_CONTROL_RQPD = (1 << 0);

// Page 1, Register 2 (Analog Gain Control): LAGN/RAGN select 0 dB or -6 dB analog gain (§8.3.5.5)
static const uint8_t PCM5122_ANALOG_GAIN_LAGN = (1 << 4);
static const uint8_t PCM5122_ANALOG_GAIN_RAGN = (1 << 0);

enum PCM5122BitsPerSample : uint8_t {
  PCM5122_BITS_PER_SAMPLE_16 = 16,
  PCM5122_BITS_PER_SAMPLE_24 = 24,
  PCM5122_BITS_PER_SAMPLE_32 = 32,
};

enum PCM5122AnalogGain : uint8_t {
  PCM5122_ANALOG_GAIN_0DB = 0x00,
  PCM5122_ANALOG_GAIN_MINUS_6DB = PCM5122_ANALOG_GAIN_LAGN | PCM5122_ANALOG_GAIN_RAGN,
};

// Page 0, Register 0x2A (DAC Data Path): AUPL/AUPR select which channel's data feeds each output (§7.4.2.42)
enum PCM5122ChannelMix : uint8_t {
  PCM5122_CHANNEL_MIX_STEREO = 0x11,      // Left data -> left out, right data -> right out
  PCM5122_CHANNEL_MIX_LEFT_ONLY = 0x12,   // Left data -> both outputs
  PCM5122_CHANNEL_MIX_RIGHT_ONLY = 0x21,  // Right data -> both outputs
  PCM5122_CHANNEL_MIX_SWAPPED = 0x22,     // Left/right outputs swapped
};

class PCM5122 final : public audio_dac::AudioDac, public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }

  void set_bits_per_sample(PCM5122BitsPerSample bits_per_sample) { this->bits_per_sample_ = bits_per_sample; }
  void set_analog_gain(PCM5122AnalogGain analog_gain) { this->analog_gain_ = analog_gain; }
  void set_channel_mix(PCM5122ChannelMix channel_mix) { this->channel_mix_ = channel_mix; }
  void set_volume_min_db(float volume_min_db) { this->volume_min_db_ = volume_min_db; }
  void set_volume_max_db(float volume_max_db) { this->volume_max_db_ = volume_max_db; }
  void set_enable_pin(GPIOPin *enable_pin) { this->enable_pin_ = enable_pin; }

  bool set_mute_off() override;
  bool set_mute_on() override;
  bool set_volume(float volume) override;

  bool is_muted() override;
  float volume() override;

  bool set_standby(bool enable);
  bool set_powerdown(bool enable);

  friend class PCM5122GPIOPin;

 protected:
  bool select_page_(uint8_t page);
  bool write_mute_();
  bool write_volume_();
  bool write_analog_gain_();
  bool write_channel_mix_();
  bool write_power_control_();

  GPIOPin *enable_pin_{nullptr};
  float volume_{1.0f};           // Matches chip post-reset DVOL default (0x30 = 0 dB)
  float volume_min_db_{-52.5f};  // Matches the previous hardcoded minimum (0x99)
  float volume_max_db_{0.0f};    // Matches the previous hardcoded maximum (0x30)
  int16_t current_page_{-1};     // -1 = unknown; cached to skip redundant page-select writes
  bool is_muted_{false};
  bool standby_{false};
  bool powerdown_{false};
  PCM5122BitsPerSample bits_per_sample_{PCM5122_BITS_PER_SAMPLE_16};
  PCM5122AnalogGain analog_gain_{PCM5122_ANALOG_GAIN_0DB};
  PCM5122ChannelMix channel_mix_{PCM5122_CHANNEL_MIX_STEREO};
};

}  // namespace esphome::pcm5122
