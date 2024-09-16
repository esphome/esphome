#include <cstddef>
#include <cstdint>

#include "bme280_i2c.h"
#include "esphome/components/i2c/i2c.h"
#include "../bme280_base/bme280_base.h"

namespace esphome {
namespace bme280_i2c {

bool BME280I2CComponent::read_byte(uint8_t a_register, uint8_t *data) {
  return I2CDevice::read_byte(a_register, data);
};
bool BME280I2CComponent::write_byte(uint8_t a_register, uint8_t data) {
  return I2CDevice::write_byte(a_register, data);
};
bool BME280I2CComponent::read_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  return I2CDevice::read_bytes(a_register, data, len);
};
bool BME280I2CComponent::read_byte_16(uint8_t a_register, uint16_t *data) {
  return I2CDevice::read_byte_16(a_register, data);
};

void BME280I2CComponent::dump_config() {
  LOG_I2C_DEVICE(this);
  BME280Component::dump_config();
}

}  // namespace bme280_i2c
}  // namespace esphome
