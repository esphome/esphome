#include "ct_clamp_sensor.h"

#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace ct_clamp {

static const char *const TAG = "ct_clamp";
static bool mutex = false;

void CTClampSensor::setup() {}

void CTClampSensor::dump_config() {
  LOG_SENSOR("", "CT Clamp Sensor", this);
  ESP_LOGCONFIG(TAG, "  Sample Duration: %.2fs", this->sample_duration_ / 1e3f);
  LOG_UPDATE_INTERVAL(this);
}

void CTClampSensor::update() {
  // Latch that an update is requested and attempts to preform update if no other clamp is being sampled.
  this->update_requested_ = true;
  if (mutex) return;
  mutex = true;
  this->update_requested_ = false;

  // Request a high loop() execution interval during sampling phase.
  this->high_freq_.start();

  // Set timeout for ending sampling phase
  this->set_timeout("read", this->sample_duration_, [this]() {
    this->is_sampling_ = false;
    this->high_freq_.stop();

    if (this->num_samples_ == 0) {
      // Shouldn't happen, but let's not crash if it does.
      this->publish_state(NAN);
      return;
    }

    float dc = this->sample_sum_ / this->num_samples_;
    float var = (this->sample_squared_sum_ / this->num_samples_) - dc * dc;
    float ac = std::sqrt(var);
    ESP_LOGD(TAG, "'%s' - Raw AC Value: %.3fA (from %d samples)", this->name_.c_str(), ac, this->num_samples_);
    this->publish_state(ac);
    mutex = false;
  });

  // Set sampling values
  this->is_sampling_ = true;
  this->num_samples_ = 0;
  this->sample_sum_ = 0.0f;
  this->sample_squared_sum_ = 0.0f;
}

void CTClampSensor::loop() {
  if (this->update_requested_) {
    this->update();
  };

  if (!this->is_sampling_)
    return;

  // Perform a single sample
  float value = this->source_->sample();
  if (isnan(value))
    return;

  this->sample_sum_ += value;
  this->sample_squared_sum_ += value * value;
  this->num_samples_++;
}

}  // namespace ct_clamp
}  // namespace esphome
