#include "alarm_control_panel.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <utility>

namespace esphome {
namespace alarm_control_panel {

static const char *const TAG = "alarm_control_panel";

AlarmControlPanelCall::AlarmControlPanelCall(AlarmControlPanel *parent) : parent_(parent) {}

AlarmControlPanelCall &AlarmControlPanelCall::set_code(const std::string &code) {
  this->code_ = code;
  return *this;
}

AlarmControlPanelCall &AlarmControlPanelCall::arm_away() {
  this->state_ = ACP_STATE_ARMED_AWAY;
  return *this;
}

AlarmControlPanelCall &AlarmControlPanelCall::arm_home() {
  this->state_ = ACP_STATE_ARMED_HOME;
  return *this;
}

AlarmControlPanelCall &AlarmControlPanelCall::arm_night() {
  this->state_ = ACP_STATE_ARMED_NIGHT;
  return *this;
}

AlarmControlPanelCall &AlarmControlPanelCall::arm_vacation() {
  this->state_ = ACP_STATE_ARMED_VACATION;
  return *this;
}

AlarmControlPanelCall &AlarmControlPanelCall::arm_custom_bypass() {
  this->state_ = ACP_STATE_ARMED_CUSTOM_BYPASS;
  return *this;
}

AlarmControlPanelCall &AlarmControlPanelCall::disarm() {
  this->state_ = ACP_STATE_DISARMED;
  return *this;
}

AlarmControlPanelCall &AlarmControlPanelCall::pending() {
  this->state_ = ACP_STATE_PENDING;
  return *this;
}

AlarmControlPanelCall &AlarmControlPanelCall::triggered() {
  this->state_ = ACP_STATE_TRIGGERED;
  return *this;
}

const optional<AlarmControlPanelState> &AlarmControlPanelCall::get_state() const { return this->state_; }
const optional<std::string> &AlarmControlPanelCall::get_code() const { return this->code_; }

void AlarmControlPanelCall::validate_() {
  if (this->state_.has_value()) {
    auto state = *this->state_;
    if (this->parent_->is_state_armed(state) && this->parent_->get_state() != ACP_STATE_DISARMED) {
      ESP_LOGW(TAG, "Cannot arm when not disarmed");
      this->state_.reset();
      return;
    }
    if (state == ACP_STATE_PENDING && this->parent_->get_state() == ACP_STATE_DISARMED) {
      ESP_LOGW(TAG, "Cannot trip alarm when not disarmed");
      this->state_.reset();
      return;
    }
    if (state == ACP_STATE_DISARMED &&
        !(this->parent_->is_state_armed(this->parent_->get_state()) ||
          this->parent_->get_state() == ACP_STATE_PENDING || this->parent_->get_state() == ACP_STATE_ARMING ||
          this->parent_->get_state() == ACP_STATE_TRIGGERED)) {
      ESP_LOGW(TAG, "Cannot disarm when not armed");
      this->state_.reset();
      return;
    }
  }
}

void AlarmControlPanelCall::perform() {
  this->validate_();
  if (this->state_) {
    this->parent_->control(*this);
  }
}

AlarmControlPanelCall AlarmControlPanel::make_call() { return {this}; }

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

void AlarmControlPanel::set_panel_state(AlarmControlPanelState state) {
  this->last_update_ = millis();
  if (state != this->current_state_) {
    auto prev_state = this->current_state_;
    ESP_LOGD(TAG, "Set state to: %s, previous: %s", this->to_string(state).c_str(),
             this->to_string(prev_state).c_str());
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

std::string AlarmControlPanel::to_string(AlarmControlPanelState state) {
  std::string str = "unknown";
  switch (state) {
    case ACP_STATE_DISARMED:
      str = "disarmed";
      break;
    case ACP_STATE_ARMED_HOME:
      str = "armed_home";
      break;
    case ACP_STATE_ARMED_AWAY:
      str = "armed_away";
      break;
    case ACP_STATE_ARMED_NIGHT:
      str = "night";
      break;
    case ACP_STATE_ARMED_VACATION:
      str = "armed_vacation";
      break;
    case ACP_STATE_ARMED_CUSTOM_BYPASS:
      str = "armed_custom_bypass";
      break;
    case ACP_STATE_PENDING:
      str = "pending";
      break;
    case ACP_STATE_ARMING:
      str = "arming";
      break;
    case ACP_STATE_DISARMING:
      str = "disarming";
      break;
    case ACP_STATE_TRIGGERED:
      str = "triggered";
      break;
  }
  return str;
}

AlarmControlPanelState AlarmControlPanel::get_state() { return this->current_state_; }

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
