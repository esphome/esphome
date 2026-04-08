#include "template_fan.h"
#include "esphome/core/log.h"

namespace esphome::template_ {

static const char *const TAG = "template.fan";

void TemplateFan::setup() {
  // Construct traits before restore so preset modes can be looked up by index
  this->traits_ =
      fan::FanTraits(this->has_oscillating_, this->speed_count_ > 0, this->has_direction_, this->speed_count_);
  this->traits_.set_supported_preset_modes(this->preset_modes_);

  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(*this);
  }
}

void TemplateFan::dump_config() { LOG_FAN("", "Template Fan", this); }

void TemplateFan::control(const fan::FanCall &call) {
  auto call_state = call.get_state();
  if (call_state.has_value())
    this->state = *call_state;
  auto call_speed = call.get_speed();
  if (call_speed.has_value() && (this->speed_count_ > 0))
    this->speed = *call_speed;
  auto call_oscillating = call.get_oscillating();
  if (call_oscillating.has_value() && this->has_oscillating_)
    this->oscillating = *call_oscillating;
  auto call_direction = call.get_direction();
  if (call_direction.has_value() && this->has_direction_)
    this->direction = *call_direction;
  this->apply_preset_mode_(call);

  this->publish_state();
}

}  // namespace esphome::template_
