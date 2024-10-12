#include "rfgain_number.h"

namespace esphome {
namespace kt0803 {

void RfGainNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_rfgain(value);
}

}  // namespace kt0803
}  // namespace esphome
