#include "refclk_frequency_number.h"

namespace esphome {
namespace si4713 {

void RefClkFrequencyNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_refclk_frequency(value);
}

}  // namespace si4713
}  // namespace esphome