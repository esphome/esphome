#include "binary_fan.h"
#include "esphome/core/log.h"

namespace esphome {
namespace binary {

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
  if (auto val = call.get_state(); val.has_value())
    this->state = *val;
  if (auto val = call.get_oscillating(); val.has_value())
    this->oscillating = *val;
  if (auto val = call.get_direction(); val.has_value())
    this->direction = *val;

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

}  // namespace binary
}  // namespace esphome
