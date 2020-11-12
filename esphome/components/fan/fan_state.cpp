#include "fan_state.h"
#include "esphome/core/log.h"

namespace esphome {
namespace fan {

FanState::FanState(const std::string &name) : Fan(name) {}

void FanState::control() { this->publish_state(); }

void FanState::setup() { this->restore_state_(); }

float FanState::get_setup_priority() const { return setup_priority::HARDWARE - 1.0f; }

}  // namespace fan
}  // namespace esphome
