#include "spa06_i2c.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::spa06_i2c {

static const char *const TAG = "spa06_i2c";

void SPA06I2CComponent::dump_config() {
  LOG_I2C_DEVICE(this);
  SPA06Component::dump_config();
}

}  // namespace esphome::spa06_i2c
