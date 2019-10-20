#include "ct_clamp_sensor.h"

#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace ct_clamp {

static const char *TAG = "ct_clamp";

<<<<<<< HEAD
=======
void CTClampSensor::setup() {
  this->is_calibrating_offset_ = true;
  this->high_freq_.start();
  this->set_timeout("calibrate_offset", this->sample_duration_, [this]() {
    this->high_freq_.stop();
    this->is_calibrating_offset_ = false;
    if (this->num_samples_ != 0) {
      this->offset_ = this->sample_sum_ / this->num_samples_;
    }
  });
}

>>>>>>> fb0f0ee785388bf0d6060556b1c912d9e51eb299
void CTClampSensor::dump_config() {
  LOG_SENSOR("", "CT Clamp Sensor", this);
  ESP_LOGCONFIG(TAG, "  Sample Duration: %.2fs", this->sample_duration_ / 1e3f);
  LOG_UPDATE_INTERVAL(this);
}

void CTClampSensor::update() {
<<<<<<< HEAD
=======
  if (this->is_calibrating_offset_)
    return;

>>>>>>> fb0f0ee785388bf0d6060556b1c912d9e51eb299
  // Update only starts the sampling phase, in loop() the actual sampling is happening.

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

    float raw = this->sample_sum_ / this->num_samples_;
    float irms = std::sqrt(raw);
    ESP_LOGD(TAG, "'%s' - Raw Value: %.2fA", this->name_.c_str(), irms);
    this->publish_state(irms);
  });

  // Set sampling values
  this->is_sampling_ = true;
  this->num_samples_ = 0;
  this->sample_sum_ = 0.0f;
}

void CTClampSensor::loop() {
<<<<<<< HEAD
  if (!this->is_sampling_)
=======
  if (!this->is_sampling_ && !this->is_calibrating_offset_)
>>>>>>> fb0f0ee785388bf0d6060556b1c912d9e51eb299
    return;

  // Perform a single sample
  float value = this->source_->sample();

<<<<<<< HEAD
=======
  if (this->is_calibrating_offset_) {
    this->sample_sum_ += value;
    this->num_samples_++;
    return;
  }

>>>>>>> fb0f0ee785388bf0d6060556b1c912d9e51eb299
  // Adjust DC offset via low pass filter (exponential moving average)
  const float alpha = 0.001f;
  this->offset_ = this->offset_ * (1 - alpha) + value * alpha;

  // Filtered value centered around the mid-point (0V)
  float filtered = value - this->offset_;

  // IRMS is sqrt(∑v_i²)
  float sq = filtered * filtered;
  this->sample_sum_ += sq;
  this->num_samples_++;
}

}  // namespace ct_clamp
}  // namespace esphome
