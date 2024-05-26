#include "esphome/components/i2c/i2c.h"
#include "bmp3xx_i2c.h"
#include <cinttypes>

namespace esphome {
namespace bmp3xx_i2c {

static const char *const TAG = "bmp3xx_i2c.sensor";

bool BMP3XXI2CComponent::read_byte(uint8_t a_register, uint8_t *data) {
  return I2CDevice::read_byte(a_register, data);
};
bool BMP3XXI2CComponent::write_byte(uint8_t a_register, uint8_t data) {
  return I2CDevice::write_byte(a_register, data);
};
bool BMP3XXI2CComponent::read_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  return I2CDevice::read_bytes(a_register, data, len);
};
bool BMP3XXI2CComponent::write_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  return I2CDevice::write_bytes(a_register, data, len);
};

void BMP3XXI2CComponent::dump_config() {
  LOG_I2C_DEVICE(this);
  BMP3XXComponent::dump_config();
}

}  // namespace bmp3xx_i2c
}  // namespace esphome
