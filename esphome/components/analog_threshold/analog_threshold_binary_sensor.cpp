#include "analog_threshold_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace analog_threshold {

static const char *const TAG = "analog_threshold.binary_sensor";

void AnalogThresholdBinarySensor::setup() {
  this->sensor_->add_on_state_callback([this](float sensor_value) {
    // if there is an invalid sensor reading, ignore the change and keep the current state
    if (!std::isnan(sensor_value)) {
      bool threshold_value = sensor_value >= (this->state ? this->lower_threshold_ : this->upper_threshold_);
      if (this->has_state()) {
        // This is not the initial state.
        this->publish_state(threshold_value);
      } else {
        this->publish_initial_state(threshold_value);
      }
    }
  });
}

void AnalogThresholdBinarySensor::set_sensor(sensor::Sensor *analog_sensor) { this->sensor_ = analog_sensor; }

void AnalogThresholdBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "Analog Threshold Binary Sensor", this);
  LOG_SENSOR("  ", "Sensor", this->sensor_);
  ESP_LOGCONFIG(TAG, "  Upper threshold: %.11f", this->upper_threshold_);
  ESP_LOGCONFIG(TAG, "  Lower threshold: %.11f", this->lower_threshold_);
}

}  // namespace analog_threshold
}  // namespace esphome
