#include "alarm_control_panel_call.h"

#include "alarm_control_panel.h"

#include "esphome/core/log.h"

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
      ESP_LOGW(TAG, "Cannot trip alarm when disarmed");
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
    if (state == ACP_STATE_ARMED_HOME && (this->parent_->get_supported_features() & ACP_FEAT_ARM_HOME) == 0) {
      ESP_LOGW(TAG, "Cannot arm home when not supported");
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

}  // namespace alarm_control_panel
}  // namespace esphome
