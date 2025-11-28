#include "gree_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gree {

static const char *const TAG = "gree.switch";

void GreeTurboSwitch::setup() {
  auto initial = this->get_initial_state_with_restore_mode();
  if (initial.has_value()) {
    this->write_state(*initial);
  }
}

void GreeTurboSwitch::dump_config() { LOG_SWITCH("  ", "Gree Turbo Switch", this); }

void GreeTurboSwitch::write_state(bool state) {
  this->parent_->set_turbo_mode(state);
}

void GreeLightSwitch::setup() {
  auto initial = this->get_initial_state_with_restore_mode();
  if (initial.has_value()) {
    this->write_state(*initial);
  }
}

void GreeLightSwitch::dump_config() { LOG_SWITCH("  ", "Gree Light Switch", this); }

void GreeLightSwitch::write_state(bool state) {
  this->parent_->set_light_mode(state);
}

void GreeHealthSwitch::setup() {
  auto initial = this->get_initial_state_with_restore_mode();
  if (initial.has_value()) {
    this->write_state(*initial);
  }
}

void GreeHealthSwitch::dump_config() { LOG_SWITCH("  ", "Gree Health Switch", this); }

void GreeHealthSwitch::write_state(bool state) {
  this->parent_->set_health_mode(state);
}

void GreeXfanSwitch::setup() {
  auto initial = this->get_initial_state_with_restore_mode();
  if (initial.has_value()) {
    this->write_state(*initial);
  }
}

void GreeXfanSwitch::dump_config() { LOG_SWITCH("  ", "Gree X-FAN Switch", this); }

void GreeXfanSwitch::write_state(bool state) {
  this->parent_->set_xfan_mode(state);
}

}  // namespace gree
}  // namespace esphome
