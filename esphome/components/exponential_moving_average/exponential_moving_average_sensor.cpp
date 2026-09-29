#include "exponential_moving_average_sensor.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::exponential_moving_average {

static const char *const TAG = "exponential_moving_average";

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
    ESP_LOGCONFIG(TAG, "  Time Constant: %" PRIu32 " ms", this->time_constant_ms_);
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
  if (std::isnan(this->accumulator_)) {
    this->publish_and_save_(value);
    return;
  }
  float alpha = this->alpha_;
  if (this->time_constant_ms_ != 0)
    alpha = 1.0f - expf(-static_cast<float>(dt) / static_cast<float>(this->time_constant_ms_));
  this->publish_and_save_(alpha * value + (1.0f - alpha) * this->accumulator_);
}

void ExponentialMovingAverageSensor::publish_and_save_(float value) {
  this->accumulator_ = value;
  this->publish_state(value);
  if (this->restore_)
    this->pref_.save(&value);
}

}  // namespace esphome::exponential_moving_average
