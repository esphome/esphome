#include "duty_time_sensor.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace duty_time_sensor {

static const char *const TAG = "duty_time_sensor";

void DutyTimeSensor::set_sensor(binary_sensor::BinarySensor *const sensor) {
  sensor->add_on_state_callback([this](bool state) { this->process_state_(state); });
}

void DutyTimeSensor::start() {
  if (!this->last_state_)
    this->process_state_(true);
}

void DutyTimeSensor::stop() {
  if (this->last_state_)
    this->process_state_(false);
}

void DutyTimeSensor::update() {
  if (this->last_state_)
    this->process_state_(true);
}

void DutyTimeSensor::loop() {
  if (this->func_ == nullptr)
    return;

  const bool state = this->func_();

  if (state != this->last_state_)
    this->process_state_(state);
}

void DutyTimeSensor::setup() {
  uint32_t seconds = 0;

  if (this->restore_) {
    this->pref_ = global_preferences->make_preference<uint32_t>(this->get_object_id_hash());
    this->pref_.load(&seconds);
  }

  this->set_value_(seconds);
}

void DutyTimeSensor::set_value_(const uint32_t sec) {
  this->edge_ms_ = 0;
  this->edge_sec_ = sec;
  this->last_update_ = millis();
  this->publish_and_save_(sec, 0);
}

void DutyTimeSensor::process_state_(const bool state) {
  const uint32_t now = millis();

  if (this->last_state_) {
    // update or falling edge
    this->counter_ms_ += now - this->last_update_;
    this->publish_and_save_(this->counter_sec_ + this->counter_ms_ / 1000, this->counter_ms_ % 1000);

    if (!state && this->last_duty_time_sensor_ != nullptr) {
      // falling edge
      const int32_t ms = this->counter_ms_ - this->edge_ms_;
      const uint32_t sec = this->counter_sec_ - this->edge_sec_;
      this->edge_ms_ = this->counter_ms_;
      this->edge_sec_ = this->counter_sec_;
      this->last_duty_time_sensor_->publish_state(sec + ms * 1e-3f);
    }
  }

  this->last_update_ = now;
  this->last_state_ = state;
}

void DutyTimeSensor::publish_and_save_(const uint32_t sec, const uint32_t ms) {
  this->counter_ms_ = ms;
  this->counter_sec_ = sec;
  this->publish_state(sec + ms * 1e-3f);

  if (this->restore_)
    this->pref_.save(&sec);
}

void DutyTimeSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Duty Time:");
  ESP_LOGCONFIG(TAG, "  Update Interval: %dms", this->get_update_interval());
  ESP_LOGCONFIG(TAG, "  Restore: %s", ONOFF(this->restore_));
  ESP_LOGCONFIG(TAG, "  Using Logical Source: %s", YESNO(this->func_ != nullptr));
  LOG_SENSOR("  ", "Duty Time Sensor:", this);
  LOG_SENSOR("  ", "Last Duty Time Sensor:", this->last_duty_time_sensor_);
}

}  // namespace duty_time_sensor
}  // namespace esphome
