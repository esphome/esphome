#include "dynamic_lamp_output.h"

namespace esphome {
namespace dynamic_lamp {

void DynamicLamp::write_state(float state) {
  this->state_ = state;
  return;
}

}  // namespace dynamic_lamp
}  // namespace esphome
