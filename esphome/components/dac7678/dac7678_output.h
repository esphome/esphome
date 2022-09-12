#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace dac7678 {

class DAC7678Output;

class DAC7678Channel : public output::FloatOutput, public Parented<DAC7678Output> {
 public:
  void set_channel(uint8_t channel) { channel_ = channel; }

 protected:
  friend class DAC7678Output;

  const uint16_t full_scale_ = 0xFFF;

  void write_state(float state) override;

  uint8_t channel_;
};

/// DAC7678 float output component.
class DAC7678Output : public Component, public i2c::I2CDevice {
 public:
  DAC7678Output() {}

  void register_channel(DAC7678Channel *channel);

  void set_internal_reference(const bool value) { this->internal_reference_ = value; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  friend DAC7678Channel;

  bool internal_reference_;

  void set_channel_value_(uint8_t channel, uint16_t value);

  uint8_t min_channel_{0xFF};
  uint8_t max_channel_{0x00};
  uint16_t dac_input_reg_[8] = {
      0,
  };
};

}  // namespace dac7678
}  // namespace esphome
