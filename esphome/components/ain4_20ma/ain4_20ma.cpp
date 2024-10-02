#include "ain4_20ma.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ain4_20ma {

static const char *const TAG = "ain4_20ma";

void Ain420maComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "AIN4-20mA Sensor:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Sensor:", this);
}

void Ain420maComponent::update() {
  uint8_t data[2];

  i2c::ErrorCode err = this->read_register(0x20, data, 2);
  if (err != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Error reading data from AIN4-20mA");
    this->publish_state(NAN);
  } else {
    uint16_t value;
    value = data[0] | (data[1] << 8);
    float current;
    current = value / 100.0;
    this->publish_state(current);
  }
}

}  // namespace ain4_20ma
}  // namespace esphome
