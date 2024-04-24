#include "template_valve.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

using namespace esphome::valve;

static const char *const TAG = "template.valve";

TemplateValve::TemplateValve()
    : open_trigger_(new Trigger<>()),
      close_trigger_(new Trigger<>),
      stop_trigger_(new Trigger<>()),
      toggle_trigger_(new Trigger<>()),
      position_trigger_(new Trigger<float>()) {}

void TemplateValve::setup() {
  ESP_LOGCONFIG(TAG, "Setting up template valve '%s'...", this->name_.c_str());
  switch (this->restore_mode_) {
    case VALVE_NO_RESTORE:
      break;
    case VALVE_RESTORE: {
      auto restore = this->restore_state_();
      if (restore.has_value())
        restore->apply(this);
      break;
    }
    case VALVE_RESTORE_AND_CALL: {
      auto restore = this->restore_state_();
      if (restore.has_value()) {
        restore->to_call(this).perform();
      }
      break;
    }
  }
}

void TemplateValve::loop() {
  bool changed = false;

  if (this->state_f_.has_value()) {
    auto s = (*this->state_f_)();
    if (s.has_value()) {
      auto pos = clamp(*s, 0.0f, 1.0f);
      if (pos != this->position) {
        this->position = pos;
        changed = true;
      }
    }
  }

  if (changed)
    this->publish_state();
}

void TemplateValve::set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
void TemplateValve::set_assumed_state(bool assumed_state) { this->assumed_state_ = assumed_state; }
void TemplateValve::set_state_lambda(std::function<optional<float>()> &&f) { this->state_f_ = f; }
float TemplateValve::get_setup_priority() const { return setup_priority::HARDWARE; }

Trigger<> *TemplateValve::get_open_trigger() const { return this->open_trigger_; }
Trigger<> *TemplateValve::get_close_trigger() const { return this->close_trigger_; }
Trigger<> *TemplateValve::get_stop_trigger() const { return this->stop_trigger_; }
Trigger<> *TemplateValve::get_toggle_trigger() const { return this->toggle_trigger_; }

void TemplateValve::dump_config() {
  LOG_VALVE("", "Template Valve", this);
  ESP_LOGCONFIG(TAG, "  Has position: %s", YESNO(this->has_position_));
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
}

void TemplateValve::control(const ValveCall &call) {
  if (call.get_stop()) {
    this->stop_prev_trigger_();
    this->stop_trigger_->trigger();
    this->prev_command_trigger_ = this->stop_trigger_;
    this->publish_state();
  }
  if (call.get_toggle().has_value()) {
    this->stop_prev_trigger_();
    this->toggle_trigger_->trigger();
    this->prev_command_trigger_ = this->toggle_trigger_;
    this->publish_state();
  }
  if (call.get_position().has_value()) {
    auto pos = *call.get_position();
    this->stop_prev_trigger_();

    if (pos == VALVE_OPEN) {
      this->open_trigger_->trigger();
      this->prev_command_trigger_ = this->open_trigger_;
    } else if (pos == VALVE_CLOSED) {
      this->close_trigger_->trigger();
      this->prev_command_trigger_ = this->close_trigger_;
    } else {
      this->position_trigger_->trigger(pos);
    }

    if (this->optimistic_) {
      this->position = pos;
    }
  }

  this->publish_state();
}

ValveTraits TemplateValve::get_traits() {
  auto traits = ValveTraits();
  traits.set_is_assumed_state(this->assumed_state_);
  traits.set_supports_stop(this->has_stop_);
  traits.set_supports_toggle(this->has_toggle_);
  traits.set_supports_position(this->has_position_);
  return traits;
}

Trigger<float> *TemplateValve::get_position_trigger() const { return this->position_trigger_; }

void TemplateValve::set_has_stop(bool has_stop) { this->has_stop_ = has_stop; }
void TemplateValve::set_has_toggle(bool has_toggle) { this->has_toggle_ = has_toggle; }
void TemplateValve::set_has_position(bool has_position) { this->has_position_ = has_position; }

void TemplateValve::stop_prev_trigger_() {
  if (this->prev_command_trigger_ != nullptr) {
    this->prev_command_trigger_->stop_action();
    this->prev_command_trigger_ = nullptr;
  }
}

}  // namespace template_
}  // namespace esphome
