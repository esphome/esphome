#include "copy_fan.h"
#include "esphome/core/log.h"

namespace esphome::copy {

static const char *const TAG = "copy.fan";

void CopyFan::setup() {
  // Copy preset modes once from source fan — stored on Fan base class
  auto source_traits = source_->get_traits();
  if (source_traits.supports_preset_modes()) {
    this->set_supported_preset_modes(source_traits.supported_preset_modes());
  }

  source_->add_on_state_callback([this]() {
    this->copy_state_from_source_();
    this->publish_state();
  });

  this->copy_state_from_source_();
  this->publish_state();
}

void CopyFan::copy_state_from_source_() {
  this->state = source_->state;
  this->oscillating = source_->oscillating;
  this->speed = source_->speed;
  this->direction = source_->direction;
  if (source_->has_preset_mode()) {
    this->set_preset_mode_(source_->get_preset_mode());
  } else {
    this->clear_preset_mode_();
  }
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
  // Preset modes are set once in setup() and wired via wire_preset_modes_()
  this->wire_preset_modes_(traits);
  return traits;
}

void CopyFan::control(const fan::FanCall &call) {
  auto call2 = source_->make_call();
  auto state = call.get_state();
  if (state.has_value())
    call2.set_state(*state);
  auto oscillating = call.get_oscillating();
  if (oscillating.has_value())
    call2.set_oscillating(*oscillating);
  auto speed = call.get_speed();
  if (speed.has_value())
    call2.set_speed(*speed);
  auto direction = call.get_direction();
  if (direction.has_value())
    call2.set_direction(*direction);
  if (call.has_preset_mode())
    call2.set_preset_mode(call.get_preset_mode());
  call2.perform();
}

}  // namespace esphome::copy
