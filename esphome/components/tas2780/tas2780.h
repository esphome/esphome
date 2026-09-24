#pragma once

#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

namespace esphome::tas2780 {

// Values are the TDM_CFG2 RX_SCFG field.
enum ChannelSelect : uint8_t { LEFT_CHANNEL = 1, RIGHT_CHANNEL = 2, MONO_DWN_MIX = 3 };

class TAS2780 : public audio_dac::AudioDac, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }
  void update() override;

  void reset();
  /// Activate with the configured power mode; re-initializes first when the mode changed.
  void activate();
  void deactivate();
  /// Write the amp level, channel and volume range to the device.
  bool apply_config();

  bool set_mute_off() override { return this->set_mute_(false); }
  bool set_mute_on() override { return this->set_mute_(true); }
  bool set_volume(float volume) override;

  bool is_muted() override { return this->is_muted_; }
  float volume() override { return this->volume_; }

  void set_amp_level(uint8_t amp_level) { this->amp_level_ = amp_level; }
  void set_power_mode(uint8_t power_mode) { this->power_mode_ = power_mode; }
  void set_vol_range_min(float min_val) { this->vol_range_min_ = min_val; }
  void set_vol_range_max(float max_val) { this->vol_range_max_ = max_val; }
  void set_selected_channel(ChannelSelect channel) { this->selected_channel_ = channel; }

 protected:
  bool select_page_(uint8_t page);
  bool update_bits_(uint8_t reg, uint8_t mask, uint8_t value);
  bool init_();
  bool reinit_();
  bool set_power_mode_(uint8_t power_mode);
  bool apply_amp_and_channel_config_();
  bool write_mode_ctrl_(uint8_t mode);
  uint8_t active_mode_() const;
  bool set_mute_(bool muted);
  bool write_volume_();
  bool log_error_states_();
  void clear_latches_();

  float volume_{0};
  float vol_range_min_{0.3f};
  float vol_range_max_{1.0f};
  uint8_t current_page_{0xFF};
  uint8_t power_mode_{2};
  uint8_t applied_power_mode_{0xFF};
  uint8_t amp_level_{8};
  ChannelSelect selected_channel_{MONO_DWN_MIX};
};

}  // namespace esphome::tas2780
