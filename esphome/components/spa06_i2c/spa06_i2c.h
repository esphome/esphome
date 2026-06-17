#pragma once
#include "esphome/components/spa06_base/spa06_base.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::spa06_i2c {

class SPA06I2CComponent : public spa06_base::SPA06Component, public i2c::I2CDevice {
 public:
  bool spa_read_byte(uint8_t a_register, uint8_t *data) override { return read_byte(a_register, data); }
  bool spa_write_byte(uint8_t a_register, uint8_t data) override { return write_byte(a_register, data); }
  bool spa_read_bytes(uint8_t a_register, uint8_t *data, size_t len) override {
    return read_bytes(a_register, data, len);
  }
  bool spa_write_bytes(uint8_t a_register, uint8_t *data, size_t len) override {
    return write_bytes(a_register, data, len);
  }
  void dump_config() override;
};

}  // namespace esphome::spa06_i2c
