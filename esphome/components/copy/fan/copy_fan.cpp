#include "copy_fan.h"
#include "esphome/core/log.h"

namespace esphome {
namespace copy {

static const char *const TAG = "copy.fan";

void CopyFan::setup() {
  source_->add_on_state_callback([this]() {
    this->state = source_->state;
    this->oscillating = source_->oscillating;
    this->speed = source_->speed;
    this->direction = source_->direction;
    this->preset_mode = source_->preset_mode;
    this->publish_state();
  });

  this->state = source_->state;
  this->oscillating = source_->oscillating;
  this->speed = source_->speed;
  this->direction = source_->direction;
  this->preset_mode = source_->preset_mode;
  this->publish_state();
}

void CopyFan::dump_config() { LOG_FAN("", "Copy Fan", this); }

fan::FanTraits CopyFan::get_traits() {
  fan::FanTraits traits;
  auto base = source_->get_traits();
  // copy traits manually so it doesn't break when new options are added
  // but the control() method hasn't implemented them yet.
  traits.set_oscillation(base.supports_oscillation());
  traits.set_speed(base.supports_speed());
  traits.set_supported_speed_count(base.supported_speed_count());
  traits.set_direction(base.supports_direction());
  traits.set_supported_preset_modes(base.supported_preset_modes());
  return traits;
}

void CopyFan::control(const fan::FanCall &call) {
  auto call2 = source_->make_call();
  if (call.get_state().has_value())
    call2.set_state(*call.get_state());
  if (call.get_oscillating().has_value())
    call2.set_oscillating(*call.get_oscillating());
  if (call.get_speed().has_value())
    call2.set_speed(*call.get_speed());
  if (call.get_direction().has_value())
    call2.set_direction(*call.get_direction());
  if (!call.get_preset_mode().empty())
    call2.set_preset_mode(call.get_preset_mode());
  call2.perform();
}

}  // namespace copy
}  // namespace esphome
