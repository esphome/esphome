#include "lis2dh12_i2c.h"
#include "esphome/core/log.h"

namespace esphome::lis2dh12_i2c {

static const char *const TAG = "lis2dh12_i2c";

void LIS2DH12I2CComponent::dump_config() {
  LOG_I2C_DEVICE(this);
  lis2dh12_base::LIS2DH12Component::dump_config();
}

}  // namespace esphome::lis2dh12_i2c
