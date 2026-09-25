#include "installation_mode_select.h"

namespace esphome::ld2460 {

void InstallationModeSelect::control(size_t index) {
  this->publish_state(index);
  this->parent_->set_installation_mode(this->option_at(index));
}

}  // namespace esphome::ld2460
