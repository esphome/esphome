#include "template_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

static const char *const TAG = "template.switch";

TemplateSwitch::TemplateSwitch() : turn_on_trigger_(new Trigger<>()), turn_off_trigger_(new Trigger<>()) {}

void TemplateSwitch::loop() {
  if (!this->f_.has_value())
    return;
  auto s = (*this->f_)();
  if (!s.has_value())
    return;

  this->publish_state(*s);
}
void TemplateSwitch::write_state(bool state) {
  if (this->prev_trigger_ != nullptr) {
    this->prev_trigger_->stop_action();
  }

  if (state) {
    this->prev_trigger_ = this->turn_on_trigger_;
    this->turn_on_trigger_->trigger();
  } else {
    this->prev_trigger_ = this->turn_off_trigger_;
    this->turn_off_trigger_->trigger();
  }

  if (this->optimistic_)
    this->publish_state(state);
}
void TemplateSwitch::set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
bool TemplateSwitch::assumed_state() { return this->assumed_state_; }
void TemplateSwitch::set_state_lambda(std::function<optional<bool>()> &&f) { this->f_ = f; }
float TemplateSwitch::get_setup_priority() const { return setup_priority::HARDWARE - 2.0f; }
Trigger<> *TemplateSwitch::get_turn_on_trigger() const { return this->turn_on_trigger_; }
Trigger<> *TemplateSwitch::get_turn_off_trigger() const { return this->turn_off_trigger_; }
void TemplateSwitch::setup() {
  optional<bool> initial_state = this->get_initial_state_with_restore_mode();

  if (initial_state.has_value()) {
    ESP_LOGD(TAG, "  Restored state %s", ONOFF(initial_state.value()));
    // if it has a value, restore_mode is not "DISABLED", therefore act on the switch:
    if (initial_state.value()) {
      this->turn_on();
    } else {
      this->turn_off();
    }
  }
}
void TemplateSwitch::dump_config() {
  LOG_SWITCH("", "Template Switch", this);
  ESP_LOGCONFIG(TAG, "  Optimistic: %s", YESNO(this->optimistic_));
}
void TemplateSwitch::set_assumed_state(bool assumed_state) { this->assumed_state_ = assumed_state; }

}  // namespace template_
}  // namespace esphome
