#include "refclk_prescaler_number.h"

namespace esphome {
namespace si4713 {

void RefClkPrescalerNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_refclk_prescaler(value);
}

}  // namespace si4713
}  // namespace esphome