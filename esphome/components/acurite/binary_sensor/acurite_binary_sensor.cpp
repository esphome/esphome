#include "esphome/core/log.h"
#include "acurite_binary_sensor.h"

namespace esphome {
namespace acurite {

static const char *const TAG = "acurite";

void AcuRiteBinarySensor::update_battery(uint8_t value) {
  if (this->battery_level_binary_sensor_) {
    this->battery_level_binary_sensor_->publish_state(value == 0);
  }
}

void AcuRiteBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "AcuRite 0x%04x:", this->id_);
  LOG_BINARY_SENSOR("  ", "Battery", this->battery_level_binary_sensor_);
}

}  // namespace acurite
}  // namespace esphome
