#include "au_enhance_switch.h"

namespace esphome {
namespace kt0803 {

void AuEnhanceSwitch::write_state(bool value) {
  this->publish_state(value);
  this->parent_->set_au_enhance(value);
}

}  // namespace kt0803
}  // namespace esphome
