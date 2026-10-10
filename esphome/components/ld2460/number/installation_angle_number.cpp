#include "installation_angle_number.h"

namespace esphome::ld2460 {

void InstallationAngleNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_installation_params(this->parent_->get_installation_height(), value);
}

}  // namespace esphome::ld2460
