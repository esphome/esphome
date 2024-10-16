#include "analog_attenuation_select.h"

namespace esphome {
namespace si4713 {

void AnalogAttenuationSelect::control(const std::string &value) {
  this->publish_state(value);
  if (auto index = this->active_index()) {
    this->parent_->set_analog_attenuation((LineAttenuation) *index);
  }
}

}  // namespace si4713
}  // namespace esphome