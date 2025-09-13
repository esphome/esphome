#include "beeper.h"

namespace esphome {
namespace haier {

void BeeperSwitch::write_state(bool state) {
  if (this->parent_->get_beeper_state() != state) {
    this->parent_->set_beeper_state(state);
  }
  this->publish_state(state);
}

}  // namespace haier
}  // namespace esphome
