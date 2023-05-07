#include "alarm_control_panel.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <utility>

namespace esphome {
namespace alarm_control_panel {

static const char *const TAG = "alarm_panel";

AlarmControlPanel::AlarmControlPanel() {}

void AlarmControlPanel::add_sensor(binary_sensor::BinarySensor *sensor, bool bypass_when_home) {
  this->sensors_.push_back(sensor);
  if (bypass_when_home) {
    this->bypass_when_home_.push_back(sensor->get_object_id_hash());
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

void AlarmControlPanel::loop() {
  // change from ARMING to ARMED_x after the arming_time_ has passed
  if (this->current_state_ == AlarmControlPanelState::ARMING) {
    auto delay = this->arming_away_time_;
    if (this->desired_state_ == AlarmControlPanelState::ARMED_HOME) {
      delay = this->arming_home_time_;
    }
    if ((millis() - this->last_update_) > delay) {
      ESP_LOGD(TAG, "After Arming Time");
      this->set_panel_state(this->desired_state_);
    }
    return;
  }
  // change from PENDING to TRIGGERED after the delay_time_ has passed
  if (this->current_state_ == AlarmControlPanelState::PENDING && (millis() - this->last_update_) > this->delay_time_) {
    ESP_LOGD(TAG, "After Delay Time");
    this->set_panel_state(AlarmControlPanelState::TRIGGERED);
    return;
  }
  auto future_state = this->current_state_;
  // reset triggered if all clear
  if (this->current_state_ == AlarmControlPanelState::TRIGGERED && this->trigger_time_ > 0 &&
      (millis() - this->last_update_) > this->trigger_time_) {
    ESP_LOGD(TAG, "After Trigger Time");
    future_state = this->desired_state_;
  }
  bool trigger = false;
  if (future_state == AlarmControlPanelState::ARMED_HOME || future_state == AlarmControlPanelState::ARMED_AWAY) {
    // TODO might be better to register change for each sensor in setup...
    for (auto *sensor : this->sensors_) {
      if (sensor->state) {
        if (this->current_state_ == AlarmControlPanelState::ARMED_HOME &&
            std::count(this->bypass_when_home_.begin(), this->bypass_when_home_.end(), sensor->get_object_id_hash())) {
          continue;
        }
        trigger = true;
        break;
      }
    }
  }
  if (trigger) {
    if (this->delay_time_ > 0 && this->current_state_ != AlarmControlPanelState::TRIGGERED) {
      this->set_panel_state(AlarmControlPanelState::PENDING);
    } else {
      this->set_panel_state(AlarmControlPanelState::TRIGGERED);
    }
  } else if (future_state != this->current_state_) {
    this->set_panel_state(future_state);
  }
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
  for (size_t i = 0; i < this->sensors_.size(); i++) {
    auto sensor = (*this->sensors_[i]);
    std::string bypass_home = "False";
    if (std::count(this->bypass_when_home_.begin(), this->bypass_when_home_.end(), sensor.get_object_id_hash())) {
      bypass_home = "True";
    }
    ESP_LOGCONFIG(TAG, "  Binary Sesnsor %u:", i);
    ESP_LOGCONFIG(TAG, "    Name: %s", sensor.get_name().c_str());
    ESP_LOGCONFIG(TAG, "    Armed home bypass: %s", bypass_home.c_str());
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
  // Home Assistant values:
  // ARM_HOME = 1
  // ARM_AWAY = 2
  // ARM_NIGHT = 4
  // TRIGGER = 8
  // ARM_CUSTOM_BYPASS = 16
  // ARM_VACATION = 32
  return 1 + 2 + 8;
}

void AlarmControlPanel::add_code(std::string code) { this->codes_.push_back(code); }

bool AlarmControlPanel::is_code_valid_(optional<std::string> code) {
  if (this->codes_.size() > 0) {
    if (code.has_value()) {
      return (std::count(this->codes_.begin(), this->codes_.end(), code.value()) == 1);
    }
    return false;
  }
  return true;
}

bool AlarmControlPanel::get_requires_code() { return this->codes_.size() > 0; }

void AlarmControlPanel::set_requires_code_to_arm(bool code_to_arm) { this->requires_code_to_arm_ = code_to_arm; }

bool AlarmControlPanel::get_requires_code_to_arm() { return this->requires_code_to_arm_; }

void AlarmControlPanel::set_arming_away_time(uint32_t time) { this->arming_away_time_ = time * 1000; }

void AlarmControlPanel::set_arming_home_time(uint32_t time) { this->arming_home_time_ = time * 1000; }

void AlarmControlPanel::set_delay_time(uint32_t time) { this->delay_time_ = time * 1000; }

void AlarmControlPanel::set_trigger_time(uint32_t time) { this->trigger_time_ = time * 1000; }

void AlarmControlPanel::disarm(optional<std::string> code) {
  ESP_LOGD(TAG, "disarm");
  if (!this->is_code_valid_(code)) {
    ESP_LOGW(TAG, "Not disarming code doesn't match");
    return;
  }
  this->desired_state_ = AlarmControlPanelState::DISARMED;
  this->set_panel_state(AlarmControlPanelState::DISARMED);
}

void AlarmControlPanel::arm_(optional<std::string> code, AlarmControlPanelState state, uint32_t delay) {
  if (this->current_state_ != AlarmControlPanelState::DISARMED) {
    ESP_LOGW(TAG, "Cannot arm when not disarmed");
    return;
  }
  if (this->requires_code_to_arm_ && !this->is_code_valid_(code)) {
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
  ESP_LOGD(TAG, "arm_away");
  this->arm_(code, AlarmControlPanelState::ARMED_AWAY, this->arming_away_time_);
}

void AlarmControlPanel::arm_home(optional<std::string> code) {
  ESP_LOGD(TAG, "arm_home");
  this->arm_(code, AlarmControlPanelState::ARMED_HOME, this->arming_home_time_);
}

void AlarmControlPanel::arm_night(optional<std::string> code) { ESP_LOGE(TAG, "arm_night not suported"); }

void AlarmControlPanel::arm_vacation(optional<std::string> code) { ESP_LOGE(TAG, "arm_vacation not suported"); }

void AlarmControlPanel::arm_custom_bypass(optional<std::string> code) {
  ESP_LOGE(TAG, "arm_custom_bypass not suported");
}

}  // namespace alarm_control_panel
}  // namespace esphome
