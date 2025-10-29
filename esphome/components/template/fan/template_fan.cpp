#include "template_fan.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

static const char *const TAG = "template.fan";

void TemplateFan::setup() {
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(*this);
  }

  // Construct traits
  this->traits_ =
      fan::FanTraits(this->has_oscillating_, this->speed_count_ > 0, this->has_direction_, this->speed_count_);
  this->traits_.set_supported_preset_modes(this->preset_modes_);
}

void TemplateFan::dump_config() { LOG_FAN("", "Template Fan", this); }

void TemplateFan::control(const fan::FanCall &call) {
  if (call.get_state().has_value())
    this->state = *call.get_state();
  if (call.get_speed().has_value() && (this->speed_count_ > 0))
    this->speed = *call.get_speed();
  if (call.get_oscillating().has_value() && this->has_oscillating_)
    this->oscillating = *call.get_oscillating();
  if (call.get_direction().has_value() && this->has_direction_)
    this->direction = *call.get_direction();
  this->preset_mode = call.get_preset_mode();

  this->publish_state();
}

}  // namespace template_
}  // namespace esphome
