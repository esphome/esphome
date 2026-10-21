#pragma once

#include <array>

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/output/float_output.h"

namespace esphome::lp5562 {

/// LP5562 4-channel (RGBW) I2C LED driver.
class LP5562Output final : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_logarithmic_dimming(bool logarithmic_dimming) { this->logarithmic_dimming_ = logarithmic_dimming; }
  /// Current limit of a channel in steps of 0.1 mA, written to the chip in setup().
  void set_channel_current(uint8_t channel, uint8_t current) { this->currents_[channel] = current; }
  void write_channel(uint8_t channel, float state);

 protected:
  // 17.5 mA, the chip's reset value
  std::array<uint8_t, 4> currents_{175, 175, 175, 175};
  bool logarithmic_dimming_{false};
};

}  // namespace esphome::lp5562
