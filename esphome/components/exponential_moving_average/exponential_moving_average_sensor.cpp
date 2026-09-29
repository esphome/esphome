#include "exponential_moving_average_sensor.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::exponential_moving_average {

static const char *const TAG = "exponential_moving_average";

const LogString *time_weighting_to_string(TimeWeighting weighting) {
  switch (weighting) {
    case TIME_WEIGHTING_PREVIOUS:
      return LOG_STR("previous");
    case TIME_WEIGHTING_LINEAR:
      return LOG_STR("linear");
    default:
      return LOG_STR("new");
  }
}

ScaledDuration scale_duration(uint32_t ms) {
  if (ms < 1000)
    return {static_cast<float>(ms), LOG_STR("ms"), 0};
  if (ms < 60 * 1000)
    return {ms / 1000.0f, LOG_STR("s"), 1};
  if (ms < 60 * 60 * 1000)
    return {ms / (60 * 1000.0f), LOG_STR("min"), 1};
  return {ms / (60 * 60 * 1000.0f), LOG_STR("h"), 1};
}

void ExponentialMovingAverageSensor::setup() {
  if (this->restore_) {
    this->pref_ = this->make_entity_preference<float>();
    float restored;
    if (this->pref_.load(&restored) && std::isfinite(restored)) {
      this->accumulator_ = restored;
      this->publish_state(restored);
    }
  }
  this->last_update_ = App.get_loop_component_start_time();
  this->source_->add_on_state_callback(
      [this](float value) { this->process_(value, App.get_loop_component_start_time()); });
}

void ExponentialMovingAverageSensor::dump_config() {
  LOG_SENSOR("", "Exponential Moving Average Sensor", this);
  if (this->time_constant_ms_ != 0) {
    const ScaledDuration time_constant = scale_duration(this->time_constant_ms_);
    ESP_LOGCONFIG(TAG,
                  "  Time Constant: %.*f %s\n"
                  "  Time Weighting: %s",
                  time_constant.decimals, time_constant.value, LOG_STR_ARG(time_constant.unit),
                  LOG_STR_ARG(time_weighting_to_string(this->time_weighting_)));
  } else {
    ESP_LOGCONFIG(TAG, "  Alpha: %.3f", this->alpha_);
  }
  ESP_LOGCONFIG(TAG, "  Restore: %s", YESNO(this->restore_));
}

void ExponentialMovingAverageSensor::reset() { this->publish_and_save_(NAN); }

void ExponentialMovingAverageSensor::process_(float value, uint32_t now) {
  if (std::isnan(value))
    return;
  // After a reboot the downtime is unknown, so the first interval is measured from setup().
  const uint32_t dt = now - this->last_update_;
  this->last_update_ = now;
  const float previous = this->previous_value_;
  this->previous_value_ = value;
  if (std::isnan(this->accumulator_)) {
    this->publish_and_save_(value);
    return;
  }
  if (this->time_constant_ms_ == 0) {
    this->publish_and_save_(this->alpha_ * value + (1.0f - this->alpha_) * this->accumulator_);
    return;
  }
  const float x = static_cast<float>(dt) / static_cast<float>(this->time_constant_ms_);
  // The share of the old average that remains after this interval.
  const float decay = expf(-x);
  // After a reboot there is no previous reading, so only the new value can be used.
  const TimeWeighting weighting = std::isnan(previous) ? TIME_WEIGHTING_NEW : this->time_weighting_;
  float result;
  switch (weighting) {
    case TIME_WEIGHTING_PREVIOUS:
      result = decay * this->accumulator_ + (1.0f - decay) * previous;
      break;
    case TIME_WEIGHTING_LINEAR: {
      // Exact result for a value moving in a straight line from the previous reading to the new one.
      const float w = x > 0.0f ? (1.0f - decay) / x : 1.0f;
      result = decay * this->accumulator_ + (w - decay) * previous + (1.0f - w) * value;
      break;
    }
    default:
      result = decay * this->accumulator_ + (1.0f - decay) * value;
      break;
  }
  this->publish_and_save_(result);
}

void ExponentialMovingAverageSensor::publish_and_save_(float value) {
  this->accumulator_ = value;
  this->publish_state(value);
  if (this->restore_)
    this->pref_.save(&value);
}

}  // namespace esphome::exponential_moving_average
