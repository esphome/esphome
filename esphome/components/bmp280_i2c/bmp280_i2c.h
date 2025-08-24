#pragma once

#include "esphome/components/bmp280_base/bmp280_base.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace bmp280_i2c {

static const char *const TAG = "bmp280_i2c.sensor";

/// This class implements support for the BMP280 Temperature+Pressure i2c sensor.
class BMP280I2CComponent : public esphome::bmp280_base::BMP280Component, public i2c::I2CDevice {
 public:
  bool read_byte_(uint8_t a_register, uint8_t *data) override { return read_byte(a_register, data); }
  bool write_byte_(uint8_t a_register, uint8_t data) override { return write_byte(a_register, data); }
  bool read_bytes_(uint8_t a_register, uint8_t *data, size_t len) override { return read_bytes(a_register, data, len); }
  bool read_byte_16_(uint8_t a_register, uint16_t *data) override { return read_byte_16(a_register, data); }
  void dump_config() override;
};

}  // namespace bmp280_i2c
}  // namespace esphome
