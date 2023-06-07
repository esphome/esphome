#include "alarm_control_panel.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <utility>

namespace esphome {
namespace alarm_control_panel {

static const char *const TAG = "alarm_panel";

AlarmControlPanelCall::AlarmControlPanelCall(AlarmControlPanel *parent) : parent_(parent) {}

AlarmControlPanelCall &AlarmControlPanelCall::set_code(std::string code) {
  this->code_ = code;
  return *this;
}

AlarmControlPanelCall &AlarmControlPanelCall::arm_away() {
  this->state_ = AlarmControlPanelState::ARMED_AWAY;
  return *this;
}

AlarmControlPanelCall &AlarmControlPanelCall::arm_home() {
  this->state_ = AlarmControlPanelState::ARMED_HOME;
  return *this;
}

AlarmControlPanelCall &AlarmControlPanelCall::arm_night() {
  this->state_ = AlarmControlPanelState::ARMED_NIGHT;
  return *this;
}

AlarmControlPanelCall &AlarmControlPanelCall::arm_vacation() {
  this->state_ = AlarmControlPanelState::ARMED_VACATION;
  return *this;
}

AlarmControlPanelCall &AlarmControlPanelCall::arm_custom_bypass() {
  this->state_ = AlarmControlPanelState::ARMED_CUSTOM_BYPASS;
  return *this;
}

AlarmControlPanelCall &AlarmControlPanelCall::disarm() {
  this->state_ = AlarmControlPanelState::DISARMED;
  return *this;
}

AlarmControlPanelCall &AlarmControlPanelCall::pending() {
  this->state_ = AlarmControlPanelState::PENDING;
  return *this;
}

AlarmControlPanelCall &AlarmControlPanelCall::triggered() {
  this->state_ = AlarmControlPanelState::TRIGGERED;
  return *this;
}

const optional<AlarmControlPanelState> &AlarmControlPanelCall::get_state() const { return this->state_; }
const optional<std::string> &AlarmControlPanelCall::get_code() const { return this->code_; }

bool is_armed_state(AlarmControlPanelState state) {
  switch (state) {
    case AlarmControlPanelState::ARMED_AWAY:
    case AlarmControlPanelState::ARMED_HOME:
    case AlarmControlPanelState::ARMED_NIGHT:
    case AlarmControlPanelState::ARMED_VACATION:
    case AlarmControlPanelState::ARMED_CUSTOM_BYPASS:
      return true;
    default:
      return false;
  }
};

void AlarmControlPanelCall::validate_() {
  if (this->state_.has_value()) {
    auto state = *this->state_;
    if (is_armed_state(state) && this->parent_->get_state() != AlarmControlPanelState::DISARMED) {
      ESP_LOGW(TAG, "Cannot arm when not disarmed");
      this->state_.reset();
      return;
    }
    if (state == AlarmControlPanelState::PENDING && this->parent_->get_state() == AlarmControlPanelState::DISARMED) {
      ESP_LOGW(TAG, "Cannot trip alarm when not disarmed");
      this->state_.reset();
      return;
    }
    if (state == AlarmControlPanelState::DISARMED &&
        !(is_armed_state(this->parent_->get_state()) || this->parent_->get_state() == AlarmControlPanelState::PENDING ||
          this->parent_->get_state() == AlarmControlPanelState::ARMING ||
          this->parent_->get_state() == AlarmControlPanelState::TRIGGERED)) {
      ESP_LOGW(TAG, "Cannot disarm when not armed");
      this->state_.reset();
      return;
    }
  }
}

void AlarmControlPanelCall::perform() {
  this->validate_();
  if (this->state_) {
    this->parent_->control_(*this);
  }
}

AlarmControlPanel::AlarmControlPanel() {}

AlarmControlPanelCall AlarmControlPanel::make_call() { return {this}; }

void AlarmControlPanel::add_sensor(binary_sensor::BinarySensor *sensor, bool bypass_when_home) {
  this->sensors_.push_back(sensor);
  if (bypass_when_home) {
    this->bypass_when_home_.push_back(sensor);
  }
};

void AlarmControlPanel::setup() {
  uint8_t value;
  this->pref_ = global_preferences->make_preference<uint8_t>(this->get_object_id_hash());
  if (this->pref_.load(&value)) {
    this->current_state_ = static_cast<AlarmControlPanelState>(value);
  } else {
    this->current_state_ = AlarmControlPanelState::DISARMED;
  }
  this->desired_state_ = this->current_state_;
}

void AlarmControlPanel::dump_config() {
  ESP_LOGCONFIG(TAG, "AlarmControlPanel:");
  ESP_LOGCONFIG(TAG, "  Current State: %s", this->to_string(this->current_state_).c_str());
  ESP_LOGCONFIG(TAG, "  Number of Codes: %u", this->codes_.size());
  ESP_LOGCONFIG(TAG, "  Requires Code To Arm: %s", this->requires_code_to_arm_ ? "Yes" : "No");
  ESP_LOGCONFIG(TAG, "  Arming Away Time: %us", (this->arming_away_time_ / 1000));
  ESP_LOGCONFIG(TAG, "  Arming Home Time: %us", (this->arming_home_time_ / 1000));
  ESP_LOGCONFIG(TAG, "  Delay Time: %us", (this->delay_time_ / 1000));
  ESP_LOGCONFIG(TAG, "  Trigger Time: %us", (this->trigger_time_ / 1000));
  ESP_LOGCONFIG(TAG, "  Supported Features: %u", this->get_supported_features());
  for (size_t i = 0; i < this->sensors_.size(); i++) {
    binary_sensor::BinarySensor *sensor = this->sensors_[i];
    std::string bypass_home = "False";
    if (std::count(this->bypass_when_home_.begin(), this->bypass_when_home_.end(), sensor)) {
      bypass_home = "True";
    }
    ESP_LOGCONFIG(TAG, "  Binary Sesnsor %u:", i);
    ESP_LOGCONFIG(TAG, "    Name: %s", (*sensor).get_name().c_str());
    ESP_LOGCONFIG(TAG, "    Armed home bypass: %s", bypass_home.c_str());
  }
}

void AlarmControlPanel::loop() {
  // change from ARMING to ARMED_x after the arming_time_ has passed
  if (this->current_state_ == AlarmControlPanelState::ARMING) {
    auto delay = this->arming_away_time_;
    if (this->desired_state_ == AlarmControlPanelState::ARMED_HOME) {
      delay = this->arming_home_time_;
    }
    if ((millis() - this->last_update_) > delay) {
      this->set_panel_state(this->desired_state_);
    }
    return;
  }
  // change from PENDING to TRIGGERED after the delay_time_ has passed
  if (this->current_state_ == AlarmControlPanelState::PENDING && (millis() - this->last_update_) > this->delay_time_) {
    this->set_panel_state(AlarmControlPanelState::TRIGGERED);
    return;
  }
  auto future_state = this->current_state_;
  // reset triggered if all clear
  if (this->current_state_ == AlarmControlPanelState::TRIGGERED && this->trigger_time_ > 0 &&
      (millis() - this->last_update_) > this->trigger_time_) {
    future_state = this->desired_state_;
  }
  bool trigger = false;
  if (is_armed_state(future_state)) {
    // TODO might be better to register change for each sensor in setup...
    for (binary_sensor::BinarySensor *sensor : this->sensors_) {
      if (sensor->state) {
        if (this->current_state_ == AlarmControlPanelState::ARMED_HOME &&
            std::count(this->bypass_when_home_.begin(), this->bypass_when_home_.end(), sensor)) {
          continue;
        }
        trigger = true;
        break;
      }
    }
  }
  if (trigger) {
    ESP_LOGD(TAG, "trigger...");
    if (this->delay_time_ > 0 && this->current_state_ != AlarmControlPanelState::TRIGGERED) {
      this->set_panel_state(AlarmControlPanelState::PENDING);
    } else {
      this->set_panel_state(AlarmControlPanelState::TRIGGERED);
    }
  } else if (future_state != this->current_state_) {
    ESP_LOGD(TAG, "need to update state...");
    this->set_panel_state(future_state);
  }
}

void AlarmControlPanel::set_panel_state(AlarmControlPanelState state) {
  this->last_update_ = millis();
  if (state != this->current_state_) {
    auto prev_state = this->current_state_;
    ESP_LOGD(TAG, "Set state to: %s, previous: %s", this->to_string(state).c_str(),
             this->to_string(prev_state).c_str());
    this->current_state_ = state;
    this->state_callback_.call();
    if (state == AlarmControlPanelState::TRIGGERED) {
      this->triggered_callback_.call();
    }
    if (prev_state == AlarmControlPanelState::TRIGGERED) {
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
    case AlarmControlPanelState::DISARMED:
      str = "disarmed";
      break;
    case AlarmControlPanelState::ARMED_HOME:
      str = "armed_home";
      break;
    case AlarmControlPanelState::ARMED_AWAY:
      str = "armed_away";
      break;
    case AlarmControlPanelState::ARMED_NIGHT:
      str = "night";
      break;
    case AlarmControlPanelState::ARMED_VACATION:
      str = "armed_vacation";
      break;
    case AlarmControlPanelState::ARMED_CUSTOM_BYPASS:
      str = "armed_custom_bypass";
      break;
    case AlarmControlPanelState::PENDING:
      str = "pending";
      break;
    case AlarmControlPanelState::ARMING:
      str = "arming";
      break;
    case AlarmControlPanelState::DISARMING:
      str = "disarming";
      break;
    case AlarmControlPanelState::TRIGGERED:
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

uint32_t AlarmControlPanel::get_supported_features() {
  auto features = AlarmControlPanelFeature::ARM_AWAY + AlarmControlPanelFeature::TRIGGER;
  if (!this->bypass_when_home_.empty()) {
    features += AlarmControlPanelFeature::ARM_HOME;
  }
  return features;
}

void AlarmControlPanel::add_code(const std::string &code) { this->codes_.push_back(code); }

bool AlarmControlPanel::is_code_valid_(optional<std::string> code) {
  if (!this->codes_.empty()) {
    if (code.has_value()) {
      ESP_LOGVV(TAG, "Checking code: %s", code.value().c_str());
      return (std::count(this->codes_.begin(), this->codes_.end(), code.value()) == 1);
    }
    ESP_LOGD(TAG, "No code provided");
    return false;
  }
  return true;
}

bool AlarmControlPanel::get_requires_code() { return !this->codes_.empty(); }

void AlarmControlPanel::set_requires_code_to_arm(bool code_to_arm) { this->requires_code_to_arm_ = code_to_arm; }

bool AlarmControlPanel::get_requires_code_to_arm() { return this->requires_code_to_arm_; }

void AlarmControlPanel::set_arming_away_time(uint32_t time) { this->arming_away_time_ = time * 1000; }

void AlarmControlPanel::set_arming_home_time(uint32_t time) { this->arming_home_time_ = time * 1000; }

void AlarmControlPanel::set_delay_time(uint32_t time) { this->delay_time_ = time * 1000; }

void AlarmControlPanel::set_trigger_time(uint32_t time) { this->trigger_time_ = time * 1000; }

void AlarmControlPanel::arm_(optional<std::string> code, AlarmControlPanelState state, uint32_t delay) {
  if (this->current_state_ != AlarmControlPanelState::DISARMED) {
    ESP_LOGW(TAG, "Cannot arm when not disarmed");
    return;
  }
  if (this->requires_code_to_arm_ && !this->is_code_valid_(std::move(code))) {
    ESP_LOGW(TAG, "Not arming code doesn't match");
    return;
  }
  this->desired_state_ = state;
  if (delay > 0) {
    this->set_panel_state(AlarmControlPanelState::ARMING);
  } else {
    this->set_panel_state(state);
  }
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

void AlarmControlPanel::control_(const AlarmControlPanelCall &call) {
  if (call.get_state()) {
    if (call.get_state() == AlarmControlPanelState::ARMED_AWAY) {
      ESP_LOGD(TAG, "arm_away");
      this->arm_(std::move(call.get_code()), AlarmControlPanelState::ARMED_AWAY, this->arming_away_time_);
    } else if (call.get_state() == AlarmControlPanelState::ARMED_HOME) {
      ESP_LOGD(TAG, "arm_home");
      this->arm_(std::move(call.get_code()), AlarmControlPanelState::ARMED_HOME, this->arming_home_time_);
    } else if (call.get_state() == AlarmControlPanelState::DISARMED) {
      ESP_LOGD(TAG, "disarm");
      if (!this->is_code_valid_(std::move(call.get_code()))) {
        ESP_LOGW(TAG, "Not disarming code doesn't match");
        return;
      }
      this->desired_state_ = AlarmControlPanelState::DISARMED;
      this->set_panel_state(AlarmControlPanelState::DISARMED);
    } else if (call.get_state() == AlarmControlPanelState::TRIGGERED) {
      ESP_LOGD(TAG, "triggered");
      this->set_panel_state(AlarmControlPanelState::TRIGGERED);
    } else if (call.get_state() == AlarmControlPanelState::PENDING) {
      ESP_LOGD(TAG, "pending");
      this->set_panel_state(AlarmControlPanelState::PENDING);
    } else {
      ESP_LOGE(TAG, "State not yet implemented: %s", this->to_string(*call.get_state()).c_str());
    }
  }
}

}  // namespace alarm_control_panel
}  // namespace esphome
