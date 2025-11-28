#include "gree_switch.h"

namespace esphome {
namespace gree {

void GreeTurboSwitch::write_state(bool state) {
  this->parent_->set_turbo_mode(state);
}

void GreeLightSwitch::write_state(bool state) {
  this->parent_->set_light_mode(state);
}

void GreeHealthSwitch::write_state(bool state) {
  this->parent_->set_health_mode(state);
}

void GreeXfanSwitch::write_state(bool state) {
  this->parent_->set_xfan_mode(state);
}

}  // namespace gree
}  // namespace esphome
