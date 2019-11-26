#include "pid_climate_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace pid {

static const char *TAG = "pid.sensor";

void PIDClimateSensor::setup() {
  this->parent_->add_on_state_callback([this]() {
    this->update_from_parent_();
  });
  this->update_from_parent_();
}
void PIDClimateSensor::update_from_parent_() {
  float value = clamp(this->parent_->get_output_value(), -1.0f, 1.0f)
  this->publish_state(value * 100.0f);
}
void PIDClimateSensor::dump_config() {
  LOG_SENSOR("", "PID Climate Sensor", this);
}

}  // namespace pid
}  // namespace esphome
