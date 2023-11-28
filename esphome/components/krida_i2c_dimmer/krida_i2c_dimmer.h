#pragma once
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace krida_i2c_dimmer {
class KridaI2CDimmer : public Component, public i2c::I2CDevice, public output::FloatOutput {
 protected:
  enum ErrorCode { NONE = 0, COMMUNICATION_FAILED } error_code_{NONE};
  uint16_t channel_address_;

 public:
  KridaI2CDimmer() : channel_address_(0x80) {}
  void set_channel(uint16_t register_address) { channel_address_ = register_address; }
  float get_setup_priority() const override { return esphome::setup_priority::BUS; }
  void setup() override;
  void dump_config() override;
  void write_state(float state) override;
};
}  // namespace krida_i2c_dimmer
}  // namespace esphome
