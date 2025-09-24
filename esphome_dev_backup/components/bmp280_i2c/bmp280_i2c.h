#pragma once

#include "esphome/components/bmp280_base/bmp280_base.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace bmp280_i2c {

static const char *const TAG = "bmp280_i2c.sensor";

/// This class implements support for the BMP280 Temperature+Pressure i2c sensor.
class BMP280I2CComponent : public esphome::bmp280_base::BMP280Component, public i2c::I2CDevice {
 public:
  bool read_byte(uint8_t a_register, uint8_t *data) override;
  bool write_byte(uint8_t a_register, uint8_t data) override;
  bool read_bytes(uint8_t a_register, uint8_t *data, size_t len) override;
  bool read_byte_16(uint8_t a_register, uint16_t *data) override;
  void dump_config() override;
};

}  // namespace bmp280_i2c
}  // namespace esphome
