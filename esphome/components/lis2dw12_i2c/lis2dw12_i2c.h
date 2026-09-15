#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/lis2dw12_base/lis2dw12_base.h"

namespace esphome::lis2dw12_i2c {

class LIS2DW12I2CComponent : public lis2dw12_base::LIS2DW12Component, public i2c::I2CDevice {
 public:
  bool read_byte(uint8_t a_register, uint8_t *data) override { return i2c::I2CDevice::read_byte(a_register, data); }
  bool write_byte(uint8_t a_register, uint8_t data) override { return i2c::I2CDevice::write_byte(a_register, data); }
  bool read_bytes(uint8_t a_register, uint8_t *data, size_t len) override {
    return i2c::I2CDevice::read_bytes(a_register, data, len);
  }
  void dump_config() override;
};

}  // namespace esphome::lis2dw12_i2c
