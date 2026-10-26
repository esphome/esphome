#include "lm75b.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome::lm75b {

ESPHOME_LOG_TAG(TAG, "lm75b");

void LM75BComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "LM75B:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this);
}

void LM75BComponent::update() {
  // Create a temporary buffer
  uint8_t buff[2];
  if (this->read_register(LM75B_REG_TEMPERATURE, buff, 2) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  // Obtain combined 16-bit value
  int16_t raw_temperature = encode_value<int16_t>(buff);
  // Read the 11-bit raw temperature value
  raw_temperature >>= 5;
  // Publish the temperature in °C
  this->publish_state(raw_temperature * 0.125);
  if (this->status_has_warning()) {
    this->status_clear_warning();
  }
}

}  // namespace esphome::lm75b
