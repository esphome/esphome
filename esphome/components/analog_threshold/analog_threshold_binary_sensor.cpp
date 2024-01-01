#include "analog_threshold_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace analog_threshold {

static const char *const TAG = "analog_threshold.binary_sensor";

void AnalogThresholdBinarySensor::setup() {
  this->sensor_->add_on_state_callback([this](float sensor_value) {
    if (std::isnan(sensor_value)) {
      // If there is an invalid sensor reading, ignore the change and keep the current state.
      // As an alternative, we could call this->invalidate_state() here, but that would be
      // a breaking change without a config option to enable it.
      return;
    }

    float threshold;
    if (!this->has_state()) {
      // No prior state, use the midpoint of the thresholds.
      threshold = (this->lower_threshold_.value() + this->upper_threshold_.value()) / 2.0f;
    } else if (this->state) {
      // Currently TRUE, use lower_threshold for comparison.
      threshold = this->lower_threshold_.value();
    } else {
      // Currently FALSE, use upper_threshold for comparison.
      threshold = this->upper_threshold_.value();
    }
    this->publish_state(sensor_value >= threshold);
  });
}

void AnalogThresholdBinarySensor::set_sensor(sensor::Sensor *analog_sensor) { this->sensor_ = analog_sensor; }

void AnalogThresholdBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "Analog Threshold Binary Sensor", this);
  LOG_SENSOR("  ", "Sensor", this->sensor_);
  ESP_LOGCONFIG(TAG,
                "  Upper threshold: %.11f\n"
                "  Lower threshold: %.11f",
                this->upper_threshold_.value(), this->lower_threshold_.value());
}

}  // namespace analog_threshold
}  // namespace esphome
