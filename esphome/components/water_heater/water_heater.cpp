#include "water_heater.h"
#include "esphome/core/log.h"

namespace esphome {
namespace water_heater {

static const char *const TAG = "water_heater";

void WaterHeater::dump_config() {
  ESP_LOGCONFIG(TAG, "Water Heater:");
  ESP_LOGCONFIG(TAG, "  Mode: %d", this->mode);
  ESP_LOGCONFIG(TAG, "  Current Temperature: %f", this->current_temperature);
  ESP_LOGCONFIG(TAG, "  Target Temperature: %f", this->target_temperature);
}

void WaterHeater::update_sensor_values_() {
  if (current_temperature_sensor_ && current_temperature_sensor_->has_state()) {
    this->current_temperature = current_temperature_sensor_->state;
  }
  if (target_temperature_sensor_ && target_temperature_sensor_->has_state()) {
    this->target_temperature = target_temperature_sensor_->state;
  }
}

}  // namespace water_heater
}  // namespace esphome
