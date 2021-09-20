#include "integration_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace integration {

static const char *const TAG = "integration";

void IntegrationSensor::setup() {
  if (this->restore_) {
    this->rtc_ = global_preferences->make_preference<float>(this->get_object_id_hash());
    float preference_value = 0;
    this->rtc_.load(&preference_value);
    this->result_ = preference_value;
  }

  this->last_update_ = millis();
  this->last_save_ = this->last_update_;

  this->publish_and_save_(this->result_);
  this->sensor_->add_on_state_callback([this](float state) { this->process_sensor_value_(state); });
}
void IntegrationSensor::dump_config() { LOG_SENSOR("", "Integration Sensor", this); }
std::string IntegrationSensor::unit_of_measurement() {
  std::string suffix;
  switch (this->time_) {
    case INTEGRATION_SENSOR_TIME_MILLISECOND:
      suffix = "ms";
      break;
    case INTEGRATION_SENSOR_TIME_SECOND:
      suffix = "s";
      break;
    case INTEGRATION_SENSOR_TIME_MINUTE:
      suffix = "min";
      break;
    case INTEGRATION_SENSOR_TIME_HOUR:
      suffix = "h";
      break;
    case INTEGRATION_SENSOR_TIME_DAY:
      suffix = "d";
      break;
  }
  std::string base = this->sensor_->get_unit_of_measurement();
  if (str_endswith(base, "/" + suffix)) {
    return base.substr(0, base.size() - suffix.size() - 1);
  }
  return base + suffix;
}
void IntegrationSensor::process_sensor_value_(float value) {
  const uint32_t now = millis();
  const double old_value = this->last_value_;
  const double new_value = value;
  const uint32_t dt_ms = now - this->last_update_;
  const double dt = dt_ms * this->get_time_factor_();
  double area = 0.0f;
  switch (this->method_) {
    case INTEGRATION_METHOD_TRAPEZOID:
      area = dt * (old_value + new_value) / 2.0;
      break;
    case INTEGRATION_METHOD_LEFT:
      area = dt * old_value;
      break;
    case INTEGRATION_METHOD_RIGHT:
      area = dt * new_value;
      break;
  }
  this->last_value_ = new_value;
  this->last_update_ = now;
  this->publish_and_save_(this->result_ + area);
}

}  // namespace integration
}  // namespace esphome
