#pragma once

#include "esphome/components/bme280_base/bme280_base.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace bme280_i2c {

static const char *const TAG = "bme280_i2c.sensor";

class BME280I2CComponent : public esphome::bme280_base::BME280Component, public i2c::I2CDevice {
  bool read_byte(uint8_t a_register, uint8_t *data) override;
  bool write_byte(uint8_t a_register, uint8_t data) override;
  bool read_bytes(uint8_t a_register, uint8_t *data, size_t len) override;
  bool read_byte_16(uint8_t a_register, uint16_t *data) override;
  void dump_config() override;
};

}  // namespace bme280_i2c
}  // namespace esphome
