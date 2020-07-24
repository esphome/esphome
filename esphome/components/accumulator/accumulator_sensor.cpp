#include "accumulator_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace accumulator {

static const char *TAG = "accumulator";

void AccumulatorSensor::setup() {
  if (this->reset_) {
    this->rtc_ = global_preferences.make_preference<float>(this->get_object_id_hash());
    this->rtc_.save(&initial_value_);
    ESP_LOGD(TAG, "Reset initial_value_ in preferences to: %f", this->initial_value_);
  } else {
    this->rtc_.load(&initial_value_);
    ESP_LOGD(TAG, "initial_value_ loaded from preferences is: %f", this->initial_value_);
  }

  last_saved_value_ = initial_value_;
  last_saved_time_ = millis();

  this->publish_state(initial_value_);
  this->sensor_->add_on_state_callback([this](float state) { this->process_sensor_value(state); });
}

void AccumulatorSensor::process_sensor_value(float value) {
  ESP_LOGD(TAG, "process_sensor_value_ Got: %f, initial_value_ = %f", value, initial_value_);

  float total = value + initial_value_;
  publish_state(total);
  save_if_needed(total);
}

void AccumulatorSensor::save_if_needed(float value) {
  uint now = millis();
  float value_delta = std::fabs(value - last_saved_value_);
  uint time_delta = now - last_saved_time_;

  if (
      // save after saveMaxTimeDelta if value has changed
      (value_delta > 0 && time_delta > max_time_interval_) ||

      // save after max_value_interval if saveMinTimeInterval has passed)
      (value_delta > max_value_interval_ && time_delta > min_time_interval_)) {
    this->rtc_.save(&value);
    last_saved_value_ = value;
    last_saved_time_ = now;

    ESP_LOGD(TAG, "value saved to preferences: %f", value);
  }
}

void AccumulatorSensor::dump_config() { LOG_SENSOR("", "Accumulator Sensor", this); }

}  // namespace accumulator
}  // namespace esphome
