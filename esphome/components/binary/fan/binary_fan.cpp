#include "binary_fan.h"
#include "esphome/core/log.h"

namespace esphome::binary {

static const char *const TAG = "binary.fan";

void BinaryFan::setup() {
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(*this);
    this->write_state_();
  }
}
void BinaryFan::dump_config() { LOG_FAN("", "Binary Fan", this); }
fan::FanTraits BinaryFan::get_traits() {
  return fan::FanTraits(this->oscillating_ != nullptr, false, this->direction_ != nullptr, 0);
}
void BinaryFan::control(const fan::FanCall &call) {
  auto state = call.get_state();
  if (state.has_value())
    this->state = *state;
  auto oscillating = call.get_oscillating();
  if (oscillating.has_value())
    this->oscillating = *oscillating;
  auto direction = call.get_direction();
  if (direction.has_value())
    this->direction = *direction;

  this->write_state_();
  this->publish_state();
}
void BinaryFan::write_state_() {
  this->output_->set_state(this->state);
  if (this->oscillating_ != nullptr)
    this->oscillating_->set_state(this->oscillating);
  if (this->direction_ != nullptr)
    this->direction_->set_state(this->direction == fan::FanDirection::REVERSE);
}

}  // namespace esphome::binary
