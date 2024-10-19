#include "tuner_frequency_number.h"

namespace esphome {
namespace si4713 {

void TunerFrequencyNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_tuner_frequency(value);
}

}  // namespace si4713
}  // namespace esphome
