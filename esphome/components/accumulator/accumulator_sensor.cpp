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

  last_saved_value = initial_value_;
  last_saved_time = millis();

  this->publish_state(initial_value_);
  this->sensor_->add_on_state_callback([this](float state) { this->process_sensor_value_(state); });
}

void AccumulatorSensor::process_sensor_value_(float value) {
  ESP_LOGD(TAG, "process_sensor_value_ Got: %f, initial_value_ = %f", value, initial_value_);

  float total = value + initial_value_;
  publish_state(total);
  SaveIfNeeded(total);
}

void AccumulatorSensor::SaveIfNeeded(float value) {
  uint currMills = millis();
  float valueDelta = abs(value - last_saved_value);
  uint timeDelta = currMills - last_saved_time;

  if ( 
      // save after saveMaxTimeDelta if value has changed
      (valueDelta > 0 && timeDelta > max_time_interval_) ||

      // save after max_value_interval if saveMinTimeInterval has passed)
      (valueDelta > max_value_interval_ && timeDelta > min_time_interval_))
  {
    this->rtc_.save(&value);
    last_saved_value = value;
    last_saved_time = currMills;

    ESP_LOGD(TAG, "value saved to preferences: %f", value);
  }
}

void AccumulatorSensor::dump_config() { LOG_SENSOR("", "Accumulator Sensor", this); }

}  // namespace accumulator
}  // namespace esphome
