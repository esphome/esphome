#include "enabled.h"

namespace esphome {
namespace loki {

void EnabledSwitch::write_state(bool state) {
  if (this->parent_->is_enabled() != state) {
    this->parent_->set_enabled(state);
  }
  this->publish_state(state);
}

}  // namespace loki
}  // namespace esphome
