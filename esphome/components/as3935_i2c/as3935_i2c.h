#pragma once

#include "esphome/components/as3935/as3935.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace as3935_i2c {

class I2CAS3935Component : public as3935::AS3935Component, public i2c::I2CDevice {
 public:
  void dump_config() override;

 protected:
  void write_register(uint8_t reg, uint8_t mask, uint8_t bits, uint8_t start_position) override;
  uint8_t read_register(uint8_t reg) override;
};

}  // namespace as3935_i2c
}  // namespace esphome
