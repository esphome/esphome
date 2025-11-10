#include "water_heater.h"
#include "esphome/core/log.h"

namespace esphome {
namespace water_heater {

static const char *const TAG = "water_heater";

void WaterHeater::setup() {}

void WaterHeater::loop() {
  this->update_sensor_values_();
}

void WaterHeater::update_sensor_values_() {
  if (current_temperature_sensor_ && current_temperature_sensor_->has_state())
    current_temperature_ = current_temperature_sensor_->state;

  if (target_temperature_sensor_ && target_temperature_sensor_->has_state())
    target_temperature_ = target_temperature_sensor_->state;
}

void WaterHeater::set_mode(WaterHeaterMode m) {
  this->mode_ = m;
  this->publish_state();
}

void WaterHeater::set_target_temperature(float t) {
  this->target_temperature_ = t;
  this->publish_state();
}

void WaterHeater::dump_config() {
  ESP_LOGCONFIG(TAG, "Water Heater:");
  ESP_LOGCONFIG(TAG, "  Mode: %s", water_heater_mode_to_string(this->mode_));
  ESP_LOGCONFIG(TAG, "  Current Temperature: %.1f °C", this->current_temperature_);
  ESP_LOGCONFIG(TAG, "  Target Temperature: %.1f °C", this->target_temperature_);
}

}  // namespace water_heater
}  // namespace esphome
