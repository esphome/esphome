#include "light_out_control_select.h"

namespace esphome::ld2412 {

void LightOutControlSelect::control(size_t index) {
  this->publish_state(index);
  this->parent_->set_light_out_control();
}

}  // namespace esphome::ld2412
