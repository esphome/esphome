#pragma once

#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::tlv320dac3100 {

class TLV320DAC3100 final : public audio_dac::AudioDac, public Component, public i2c::I2CDevice {
 public:
  /// setup_table (PROGMEM) configures the clocks and outputs; the bytes power up the DAC data path and drivers.
  TLV320DAC3100(const i2c::I2CRegValue *setup_table, uint8_t setup_count, uint8_t data_path, uint8_t hp_driver,
                uint8_t spk_driver)
      : data_path_(data_path),
        hp_driver_(hp_driver),
        spk_driver_(spk_driver),
        setup_count_(setup_count),
        setup_table_(setup_table) {}

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

  const uint8_t data_path_;
  const uint8_t hp_driver_;
  const uint8_t spk_driver_;
  const uint8_t setup_count_;
  bool outputs_ready_{false};
  bool clock_running_{false};
  bool dac_powered_{false};
  const i2c::I2CRegValue *const setup_table_;
  GPIOPin *reset_pin_{nullptr};
  float volume_{1.0f};
};

}  // namespace esphome::tlv320dac3100
