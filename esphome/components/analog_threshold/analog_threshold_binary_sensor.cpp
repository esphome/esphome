#include "analog_threshold_binary_sensor.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace analog_threshold {

static const char *const TAG = "analog_threshold.binary_sensor";

void AnalogThresholdBinarySensor::setup() {
  this->last_change_ = millis();
  this->last_above_ = this->sensor_->get_state() >= (this->lower_threshold_ + this->upper_threshold_) / 2.0f;
  this->publish_initial_state(this->last_above_ != this->inverted_);
}

void AnalogThresholdBinarySensor::loop() {
  const auto now = millis();

  // check if we changed side
  if (this->last_above_ != this->is_above_()) {
    this->last_change_ = now;
    this->last_above_ = !this->last_above_;
  }

  // check delay
  if (now - this->last_change_ >= (this->last_above_ ? this->delay_high_ : this->delay_low_)) {
    // change state
    this->publish_state(this->last_above_ != this->inverted_);
  }
}
void AnalogThresholdBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "Analog Threshold Binary Sensor", this);
  LOG_SENSOR("  ", "Sensor", this->sensor_);
  ESP_LOGCONFIG(TAG, "  Upper threshold: %.11f", this->upper_threshold_);
  ESP_LOGCONFIG(TAG, "  Lower threshold: %.11f", this->lower_threshold_);

  ESP_LOGCONFIG(TAG, "  Delay High: %.3fs", this->delay_high_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Delay Low: %.3fs", this->delay_low_ / 1e3f);
  ESP_LOGCONFIG(TAG, "  Inverted: %s", YESNO(this->inverted_));
}

bool AnalogThresholdBinarySensor::is_above_() const {
  return this->sensor_->get_state() >= (this->last_above_ ? this->lower_threshold_ : this->upper_threshold_);
}

}  // namespace analog_threshold
}  // namespace esphome
