#include "fan_state.h"

namespace esphome {
namespace fan {

static const char *const TAG = "fan";

void FanState::setup() {
  auto restore = this->restore_state_();
  restore.to_call(*this).perform();
}
float FanState::get_setup_priority() const { return setup_priority::DATA - 1.0f; }

}  // namespace fan
}  // namespace esphome
