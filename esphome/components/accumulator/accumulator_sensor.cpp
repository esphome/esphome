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
  ESP_LOGD(TAG, "process_sensor_value Got: %f, initial_value = %f", value, initial_value_);

  float total = value + initial_value_;
  publish_state(total);
  if (needs_save(total)) {
    save(total);
  }
}

bool AccumulatorSensor::needs_save(float value) {
  uint now = millis();
  float value_delta = std::fabs(value - last_saved_value_);
  uint time_since_last_save = now - last_saved_time_;

  return (value_delta != 0) && ((time_since_last_save >= save_min_interval_ && value_delta >= save_on_value_delta_) ||
                                (time_since_last_save >= save_max_interval_ && save_max_interval_ != 0));
}

void AccumulatorSensor::save(float value) {
  this->rtc_.save(&value);
  last_saved_value_ = value;
  last_saved_time_ = millis();

  ESP_LOGD(TAG, "Value saved to preferences: %f", value);
}

void AccumulatorSensor::dump_config() { LOG_SENSOR("", "Accumulator Sensor", this); }

}  // namespace accumulator
}  // namespace esphome
