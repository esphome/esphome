#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/lis2dh12_base/lis2dh12_base.h"

namespace esphome::lis2dh12_i2c {

class LIS2DH12I2CComponent : public lis2dh12_base::LIS2DH12Component, public i2c::I2CDevice {
 public:
  bool read_byte(uint8_t a_register, uint8_t *data) override { return i2c::I2CDevice::read_byte(a_register, data); }
  bool write_byte(uint8_t a_register, uint8_t data) override { return i2c::I2CDevice::write_byte(a_register, data); }
  // LIS2DH12 I2C multi-byte reads require MSB set for auto-increment
  bool read_bytes(uint8_t a_register, uint8_t *data, size_t len) override {
    return i2c::I2CDevice::read_bytes(a_register | 0x80, data, len);
  }

  void dump_config() override;
};

}  // namespace esphome::lis2dh12_i2c
