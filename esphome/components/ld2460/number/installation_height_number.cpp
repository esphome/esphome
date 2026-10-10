#include "installation_height_number.h"

namespace esphome::ld2460 {

void InstallationHeightNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_installation_params(value, this->parent_->get_installation_angle());
}

}  // namespace esphome::ld2460
