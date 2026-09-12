#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/output/float_output.h"

namespace esphome::lp5562 {

class LP5562Output;

class LP5562Channel final : public output::FloatOutput, public Parented<LP5562Output> {
 public:
  void set_channel(uint8_t channel) { channel_ = channel; }

 protected:
  friend class LP5562Output;

  void write_state(float state) override;

  uint8_t channel_;
};

/// LP5562 4-channel (RGBW) I2C LED driver.
class LP5562Output final : public Component, public i2c::I2CDevice {
 public:
  void register_channel(LP5562Channel *channel);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  friend class LP5562Channel;

  void set_channel_value_(uint8_t channel, uint8_t value);
};

}  // namespace esphome::lp5562
