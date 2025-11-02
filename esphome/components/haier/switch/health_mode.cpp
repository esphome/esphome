#include "health_mode.h"

namespace esphome {
namespace haier {

void HealthModeSwitch::write_state(bool state) {
  if (this->parent_->get_health_mode() != state) {
    this->parent_->set_health_mode(state);
  }
  this->publish_state(state);
}

}  // namespace haier
}  // namespace esphome
