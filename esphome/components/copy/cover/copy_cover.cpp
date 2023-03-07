#include "copy_cover.h"
#include "esphome/core/log.h"

namespace esphome {
namespace copy {

static const char *const TAG = "copy.cover";

void CopyCover::setup() {
  source_->add_on_state_callback([this]() {
    this->current_operation = this->source_->current_operation;
    this->position = this->source_->position;
    this->tilt = this->source_->tilt;
    this->publish_state();
  });

  this->current_operation = this->source_->current_operation;
  this->position = this->source_->position;
  this->tilt = this->source_->tilt;
  this->publish_state();
}

void CopyCover::dump_config() { LOG_COVER("", "Copy Cover", this); }

cover::CoverTraits CopyCover::get_traits() {
  auto base = source_->get_traits();
  cover::CoverTraits traits{};
  // copy traits manually so it doesn't break when new options are added
  // but the control() method hasn't implemented them yet.
  traits.set_is_assumed_state(base.get_is_assumed_state());
  traits.set_supports_position(base.get_supports_position());
  traits.set_supports_tilt(base.get_supports_tilt());
  traits.set_supports_toggle(base.get_supports_toggle());
  return traits;
}

void CopyCover::control(const cover::CoverCall &call) {
  auto call2 = source_->make_call();
  call2.set_stop(call.get_stop());
  if (call.get_tilt().has_value())
    call2.set_tilt(*call.get_tilt());
  if (call.get_position().has_value())
    call2.set_position(*call.get_position());
  if (call.get_tilt().has_value())
    call2.set_tilt(*call.get_tilt());
  call2.perform();
}

}  // namespace copy
}  // namespace esphome
