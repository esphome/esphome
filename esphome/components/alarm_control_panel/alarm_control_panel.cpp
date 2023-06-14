#include <utility>

#include "alarm_control_panel.h"

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace alarm_control_panel {

static const char *const TAG = "alarm_control_panel";

AlarmControlPanelCall AlarmControlPanel::make_call() { return AlarmControlPanelCall(this); }

bool AlarmControlPanel::is_state_armed(AlarmControlPanelState state) {
  switch (state) {
    case ACP_STATE_ARMED_AWAY:
    case ACP_STATE_ARMED_HOME:
    case ACP_STATE_ARMED_NIGHT:
    case ACP_STATE_ARMED_VACATION:
    case ACP_STATE_ARMED_CUSTOM_BYPASS:
      return true;
    default:
      return false;
  }
};

void AlarmControlPanel::publish_state(AlarmControlPanelState state) {
  this->last_update_ = millis();
  if (state != this->current_state_) {
    auto prev_state = this->current_state_;
    ESP_LOGD(TAG, "Set state to: %s, previous: %s", LOG_STR_ARG(alarm_control_panel_state_to_string(state)),
             LOG_STR_ARG(alarm_control_panel_state_to_string(prev_state)));
    this->current_state_ = state;
    this->state_callback_.call();
    if (state == ACP_STATE_TRIGGERED) {
      this->triggered_callback_.call();
    }
    if (prev_state == ACP_STATE_TRIGGERED) {
      this->cleared_callback_.call();
    }
    if (state == this->desired_state_) {
      // only store when in the desired state
      this->pref_.save(&state);
    }
  }
}

void AlarmControlPanel::add_on_state_callback(std::function<void()> &&callback) {
  this->state_callback_.add(std::move(callback));
}

void AlarmControlPanel::add_on_triggered_callback(std::function<void()> &&callback) {
  this->triggered_callback_.add(std::move(callback));
}

void AlarmControlPanel::add_on_cleared_callback(std::function<void()> &&callback) {
  this->cleared_callback_.add(std::move(callback));
}

void AlarmControlPanel::arm_away(optional<std::string> code) {
  auto call = this->make_call();
  call.arm_away();
  if (code.has_value())
    call.set_code(code.value());
  call.perform();
}

void AlarmControlPanel::arm_home(optional<std::string> code) {
  auto call = this->make_call();
  call.arm_home();
  if (code.has_value())
    call.set_code(code.value());
  call.perform();
}

void AlarmControlPanel::arm_night(optional<std::string> code) {
  auto call = this->make_call();
  call.arm_night();
  if (code.has_value())
    call.set_code(code.value());
  call.perform();
}

void AlarmControlPanel::arm_vacation(optional<std::string> code) {
  auto call = this->make_call();
  call.arm_vacation();
  if (code.has_value())
    call.set_code(code.value());
  call.perform();
}

void AlarmControlPanel::arm_custom_bypass(optional<std::string> code) {
  auto call = this->make_call();
  call.arm_custom_bypass();
  if (code.has_value())
    call.set_code(code.value());
  call.perform();
}

void AlarmControlPanel::disarm(optional<std::string> code) {
  auto call = this->make_call();
  call.disarm();
  if (code.has_value())
    call.set_code(code.value());
  call.perform();
}

}  // namespace alarm_control_panel
}  // namespace esphome
