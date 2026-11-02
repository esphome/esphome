#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::dac7678 {

/// DAC7678 float output component.
class DAC7678Output final : public Component, public i2c::I2CDevice {
 public:
  DAC7678Output() {}

  void register_channel(uint8_t channel);
  void write_channel(uint8_t channel, float state);

  void set_internal_reference(const bool value) { this->internal_reference_ = value; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  bool internal_reference_;

  uint8_t min_channel_{0xFF};
  uint8_t max_channel_{0x00};
  uint16_t dac_input_reg_[8] = {
      0,
  };
};

}  // namespace esphome::dac7678
