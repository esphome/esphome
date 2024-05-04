#include <cstddef>
#include <cstdint>

#include "ens160_i2c.h"
#include "esphome/components/i2c/i2c.h"
#include "../ens160_base/ens160_base.h"

namespace esphome {
namespace ens160_i2c {

static const char *const TAG = "ens160_i2c.sensor";

bool ENS160I2CComponent::read_byte(uint8_t a_register, uint8_t *data) {
  return I2CDevice::read_byte(a_register, data);
};
bool ENS160I2CComponent::write_byte(uint8_t a_register, uint8_t data) {
  return I2CDevice::write_byte(a_register, data);
};
bool ENS160I2CComponent::read_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  return I2CDevice::read_bytes(a_register, data, len);
};
bool ENS160I2CComponent::write_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  return I2CDevice::write_bytes(a_register, data, len);
};

void ENS160I2CComponent::dump_config() {
  ENS160Component::dump_config();
  LOG_I2C_DEVICE(this);
}

}  // namespace ens160_i2c
}  // namespace esphome
