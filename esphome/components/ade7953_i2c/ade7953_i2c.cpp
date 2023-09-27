#include "ade7953_i2c.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ade7953_i2c {

static const char *const TAG = "ade7953";

void ADE7953_i2c::dump_config() {
  ESP_LOGCONFIG(TAG, "ADE7953_i2c:");
  LOG_I2C_DEVICE(this);
  ade7953_base::ADE7953::dump_config();
}


}  // namespace ade7953_i2c
}  // namespace esphome