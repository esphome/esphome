#include "lis2dw12_i2c.h"
#include "esphome/core/log.h"

namespace esphome::lis2dw12_i2c {

ESPHOME_LOG_TAG(TAG, "lis2dw12_i2c");

void LIS2DW12I2CComponent::dump_config() {
  LOG_I2C_DEVICE(this);
  lis2dw12::LIS2DW12Component::dump_config();
}

}  // namespace esphome::lis2dw12_i2c
