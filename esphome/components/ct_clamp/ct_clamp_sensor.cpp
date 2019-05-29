#include "ct_clamp_sensor.h"

#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace ct_clamp {

static const char *TAG = "ct_clamp";

void CTClampSensor::dump_config() {
  LOG_SENSOR("", "CT Clamp Sensor", this);
  ESP_LOGCONFIG(TAG, "  Sample Size: %u", this->sample_size_);

  LOG_UPDATE_INTERVAL(this);
}

void CTClampSensor::update() {
  float sum = 0;
  for (unsigned int n = 0; n < this->sample_size_; n++) {
    float value = this->source_->sample();

    // Shift center offset based on values coming in.
    offset_ = offset_ + (value - offset_) / 1024.0;
    // Digital low pass filter to center on 0V.
    float filtered = value - offset_;

    // Root-mean-square method current
    // 1) square current values
    float sq = filtered * filtered;
    // 2) sum
    sum += sq;
  }

  float irms = std::sqrt(sum / this->sample_size_);

  ESP_LOGD(TAG, "'%s' - Raw Value: %.2f", this->name_.c_str(), irms);

  this->publish_state(irms);
}

}  // namespace ct_clamp
}  // namespace esphome
