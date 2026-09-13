#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace m5stack_4relay {

static constexpr uint8_t UNIT_4RELAY_REG = 0X10;
static constexpr uint8_t UNIT_4RELAY_RELAY_REG = 0X11;

class M5Stack4Relay : public Component, public i2c::I2CDevice {
 public:
  void set_switch_mode(bool mode);

  void relay_write(uint8_t number, bool state);

 protected:
  void write1_byte_(uint8_t register_address, uint8_t data);
  uint8_t read1_byte_(uint8_t register_address);

  void dump_config() override;

  void init_(bool mode);

  void setup() override;
};

}  // namespace m5stack_4relay
}  // namespace esphome
