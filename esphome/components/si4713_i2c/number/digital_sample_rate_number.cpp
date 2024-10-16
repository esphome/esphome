#include "digital_sample_rate_number.h"

namespace esphome {
namespace si4713 {

void DigitalSampleRateNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_digital_sample_rate(value);
}

}  // namespace si4713
}  // namespace esphome