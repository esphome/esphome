#include "i2c_device.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cinttypes>

namespace esphome::i2c_device {

ESPHOME_LOG_TAG(TAG, "i2c_device");

void I2CDeviceComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "I2CDevice");
  LOG_I2C_DEVICE(this);
}

}  // namespace esphome::i2c_device
