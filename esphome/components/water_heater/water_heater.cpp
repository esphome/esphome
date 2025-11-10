#include "water_heater.h"
#include "esphome/core/log.h"

namespace esphome {
namespace water_heater {

static const char *const TAG = "water_heater";

void WaterHeater::loop() {
  this->update_values_from_sensors_();
}

void WaterHeater::update_values_from_sensors_() {
  if (current_temperature_sensor_ && current_temperature_sensor_->has_state())
    this->current_temperature = current_temperature_sensor_->state;

  if (target_temperature_sensor_ && target_temperature_sensor_->has_state())
    this->target_temperature = target_temperature_sensor_->state;
}

void WaterHeater::set_mode(WaterHeaterMode m) {
  this->mode = m;
  this->publish_state();
}

void WaterHeater::set_target_temperature(float t) {
  this->target_temperature = t;
  this->publish_state();
}

void WaterHeater::dump_config() {
  ESP_LOGCONFIG(TAG, "Water Heater:");
  ESP_LOGCONFIG(TAG, "  Mode: %d", this->mode);
  ESP_LOGCONFIG(TAG, "  Current Temperature: %.1f", this->current_temperature);
  ESP_LOGCONFIG(TAG, "  Target Temperature: %.1f", this->target_temperature);
  ESP_LOGCONFIG(TAG, "  Min Temp: %.1f", this->traits.get_visual_min_temperature());
  ESP_LOGCONFIG(TAG, "  Max Temp: %.1f", this->traits.get_visual_max_temperature());
}

}  // namespace water_heater
}  // namespace esphome
