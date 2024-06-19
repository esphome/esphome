#include "duty_time_sensor.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace duty_time_sensor {

static const char *const TAG = "duty_time_sensor";

#ifdef USE_BINARY_SENSOR
void DutyTimeSensor::set_sensor(binary_sensor::BinarySensor *const sensor) {
  sensor->add_on_state_callback([this](bool state) { this->process_state_(state); });
}
#endif

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
  this->last_time_ = 0;
  if (this->last_state_)
    this->last_time_ = millis();  // last time with 0 ms correction
  this->publish_and_save_(sec, 0);
}

void DutyTimeSensor::process_state_(const bool state) {
  const uint32_t now = millis();

  if (this->last_state_) {
    // update or falling edge
    const uint32_t tm = now - this->last_time_;
    const uint32_t ms = tm % 1000;

    this->publish_and_save_(this->total_sec_ + tm / 1000, ms);
    this->last_time_ = now - ms;  // store time with ms correction

    if (!state) {
      // falling edge
      this->last_time_ = ms;  // temporary store ms correction only
      this->last_state_ = false;

      if (this->last_duty_time_sensor_ != nullptr) {
        const uint32_t turn_on_ms = now - this->edge_time_;
        this->last_duty_time_sensor_->publish_state(turn_on_ms * 1e-3f);
      }
    }

  } else if (state) {
    // rising edge
    this->last_time_ = now - this->last_time_;  // store time with ms correction
    this->edge_time_ = now;                     // store turn-on start time
    this->last_state_ = true;
  }
}

void DutyTimeSensor::publish_and_save_(const uint32_t sec, const uint32_t ms) {
  this->total_sec_ = sec;
  this->publish_state(sec + ms * 1e-3f);

  if (this->restore_)
    this->pref_.save(&sec);
}

void DutyTimeSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Duty Time:");
  ESP_LOGCONFIG(TAG, "  Update Interval: %" PRId32 "ms", this->get_update_interval());
  ESP_LOGCONFIG(TAG, "  Restore: %s", ONOFF(this->restore_));
  LOG_SENSOR("  ", "Duty Time Sensor:", this);
  LOG_SENSOR("  ", "Last Duty Time Sensor:", this->last_duty_time_sensor_);
}

}  // namespace duty_time_sensor
}  // namespace esphome
