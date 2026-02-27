#include "bmp280_i2c.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bmp280_i2c {

void BMP280I2CComponent::dump_config() {
  LOG_I2C_DEVICE(this);
  BMP280Component::dump_config();
}

}  // namespace bmp280_i2c
}  // namespace esphome
