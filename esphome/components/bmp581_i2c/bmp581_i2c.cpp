#include "bmp581_i2c.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::bmp581_i2c {

void BMP581I2CComponent::dump_config() {
  LOG_I2C_DEVICE(this);
  BMP581Component::dump_config();
}

}  // namespace esphome::bmp581_i2c
