#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

namespace esphome {
namespace m5stack_8angle {

static const uint8_t M5STACK_8ANGLE_REGISTER_ANALOG_INPUT_12B = 0x00;
static const uint8_t M5STACK_8ANGLE_REGISTER_ANALOG_INPUT_8B = 0x10;
static const uint8_t M5STACK_8ANGLE_REGISTER_DIGITAL_INPUT = 0x20;
static const uint8_t M5STACK_8ANGLE_REGISTER_RGB_24B = 0x30;
static const uint8_t M5STACK_8ANGLE_REGISTER_FW_VERSION = 0xFE;

enum AnalogBits : uint8_t {
  BITS_8 = 8,
  BITS_12 = 12,
};

class M5Stack8AngleComponent : public i2c::I2CDevice, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  float read_knob_pos(uint8_t channel, AnalogBits bits = AnalogBits::BITS_8);
  int32_t read_knob_pos_raw(uint8_t channel, AnalogBits bits = AnalogBits::BITS_8);
  int8_t read_switch();

 protected:
  uint8_t fw_version_;
};

}  // namespace m5stack_8angle
}  // namespace esphome
