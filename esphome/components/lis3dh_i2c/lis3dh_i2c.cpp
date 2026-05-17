#include "lis3dh_i2c.h"
#include "esphome/core/log.h"

namespace esphome {
namespace lis3dh_i2c {

static const char *const TAG = "lis3dh_i2c";

void LIS3DHI2C::dump_config() {
  LIS3DHComponent::dump_config();
  LOG_I2C_DEVICE(this);
}

bool LIS3DHI2C::read_register(uint8_t reg, uint8_t *data, uint16_t len) {
  // Bit 7 set enables address auto-increment for multi-byte transfers.
  if (len > 1) {
    reg |= 0x80;
  }
  return this->read_bytes(reg, data, len);
}

bool LIS3DHI2C::write_register(uint8_t reg, const uint8_t *data, uint16_t len) {
  if (len > 1) {
    reg |= 0x80;
  }
  return this->write_bytes(reg, data, len);
}

}  // namespace lis3dh_i2c
}  // namespace esphome
