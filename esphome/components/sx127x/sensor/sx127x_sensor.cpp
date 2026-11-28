#include "sx127x_sensor.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cmath>

namespace esphome::sx127x {

ESPHOME_LOG_TAG(TAG, "sx127x.sensor");

void SX127xSensor::setup() {
  this->samples_.init(this->max_samples_);
  this->set_interval(this->sample_interval_ms_, [this]() { this->sample_(); });
}

void SX127xSensor::sample_() {
  if (this->samples_.full()) {
    return;
  }
  const float rssi = this->parent_->get_rssi_instant();
  if (!std::isnan(rssi)) {
    this->samples_.push_back(static_cast<int16_t>(lroundf(rssi * 2)));
  }
}

void SX127xSensor::update() {
  if (this->noise_floor_sensor_ == nullptr) {
    return;
  }
  if (this->samples_.empty()) {
    // Not listening for the whole interval
    this->noise_floor_sensor_->publish_state(NAN);
    return;
  }
  auto *middle = this->samples_.begin() + this->samples_.size() / 2;
  std::nth_element(this->samples_.begin(), middle, this->samples_.end());
  this->noise_floor_sensor_->publish_state(*middle / 2.0f);
  this->samples_.clear();
}

void SX127xSensor::dump_config() {
  ESP_LOGCONFIG(TAG,
                "SX127X Sensor:\n"
                "  Sample Interval: %" PRIu32 " ms",
                this->sample_interval_ms_);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Noise Floor", this->noise_floor_sensor_);
}

}  // namespace esphome::sx127x
