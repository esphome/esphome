#include "bmp280_i2c.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bmp280_i2c {

bool BMP280I2CComponent::read_byte(uint8_t a_register, uint8_t *data) {
  return I2CDevice::read_byte(a_register, data);
};
bool BMP280I2CComponent::write_byte(uint8_t a_register, uint8_t data) {
  return I2CDevice::write_byte(a_register, data);
};
bool BMP280I2CComponent::read_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  return I2CDevice::read_bytes(a_register, data, len);
};
bool BMP280I2CComponent::read_byte_16(uint8_t a_register, uint16_t *data) {
  return I2CDevice::read_byte_16(a_register, data);
};

void BMP280I2CComponent::dump_config() {
  LOG_I2C_DEVICE(this);
  BMP280Component::dump_config();
}

}  // namespace bmp280_i2c
}  // namespace esphome
