#pragma once

#include "esphome/components/bmp581_base/bmp581_base.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::bmp581_i2c {

static const char *const TAG = "bmp581_i2c.sensor";

/// This class implements support for the BMP581 Temperature+Pressure i2c sensor.
class BMP581I2CComponent : public esphome::bmp581_base::BMP581Component, public i2c::I2CDevice {
 public:
  bool bmp_read_byte(uint8_t a_register, uint8_t *data) override { return read_byte(a_register, data); }
  bool bmp_write_byte(uint8_t a_register, uint8_t data) override { return write_byte(a_register, data); }
  bool bmp_read_bytes(uint8_t a_register, uint8_t *data, size_t len) override {
    return read_bytes(a_register, data, len);
  }
  bool bmp_write_bytes(uint8_t a_register, uint8_t *data, size_t len) override {
    return write_bytes(a_register, data, len);
  }
  void dump_config() override;
};

}  // namespace esphome::bmp581_i2c
