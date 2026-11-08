#pragma once

#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::tlv320dac3100 {

class TLV320DAC3100 final : public audio_dac::AudioDac, public Component, public i2c::I2CDevice {
 public:
  /// PROGMEM table: setup_count entries for the clocks and outputs, then the DAC data path and driver power-up.
  TLV320DAC3100(const i2c::I2CRegValue *table, uint8_t setup_count) : setup_count_(setup_count), table_(table) {}

  void setup() override;
  void dump_config() override;

  bool set_mute_off() override;
  bool set_mute_on() override;
  bool set_volume(float volume) override;

  bool is_muted() override { return this->is_muted_; }
  float volume() override { return this->volume_; }
  void on_audio_started() override;

  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }

 protected:
  bool write_levels_();
  void power_up_dac_();
  void unmute_outputs_();

  const uint8_t setup_count_;
  bool outputs_ready_{false};
  bool clock_running_{false};
  bool dac_powered_{false};
  bool dac_settling_{false};
  const i2c::I2CRegValue *const table_;
  GPIOPin *reset_pin_{nullptr};
  float volume_{1.0f};
};

}  // namespace esphome::tlv320dac3100
