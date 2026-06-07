#pragma once

#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::tas2780 {

enum ChannelSelect : uint8_t { MONO_DWN_MIX, LEFT_CHANNEL, RIGHT_CHANNEL };

class TAS2780 : public audio_dac::AudioDac, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void update() override;

  void reset();
  void activate(uint8_t power_mode = 2);
  void deactivate();
  void apply_amp_and_channel_config();

  bool set_mute_off() override;
  bool set_mute_on() override;
  bool set_volume(float volume) override;

  bool is_muted() override;
  float volume() override;

  void set_amp_level(uint8_t amp_level) { this->amp_level_ = amp_level; }
  void set_power_mode(uint8_t power_mode) { this->power_mode_ = power_mode; }
  void set_vol_range_min(float min_val) { this->vol_range_min_ = min_val; }
  void set_vol_range_max(float max_val) { this->vol_range_max_ = max_val; }
  void set_selected_channel(ChannelSelect channel) { this->selected_channel_ = channel; }

 protected:
  void init_();
  void set_power_mode_(uint8_t power_mode);
  bool write_mode_ctrl_(uint8_t mode);
  bool write_mute_();
  bool write_volume_();
  void log_error_states_();

  float volume_{0};
  float vol_range_min_{0.3f};
  float vol_range_max_{1.0f};
  uint8_t power_mode_{2};
  uint8_t amp_level_{8};
  ChannelSelect selected_channel_{MONO_DWN_MIX};
};

}  // namespace esphome::tas2780
