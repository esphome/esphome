#include "tuner_enable_switch.h"

namespace esphome {
namespace si4713 {

void TunerEnableSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_tuner_enable(value);
}

}  // namespace si4713
}  // namespace esphome