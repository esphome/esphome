#include "digital_sample_bits_select.h"

namespace esphome {
namespace si4713 {

void DigitalSampleBitsSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_digital_sample_bits((SampleBits) *index);
  }
}

}  // namespace si4713
}  // namespace esphome