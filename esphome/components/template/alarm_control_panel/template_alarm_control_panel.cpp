#include "template_alarm_control_panel.h"
#include "esphome/components/alarm_control_panel/alarm_control_panel.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <utility>

namespace esphome {
namespace template_ {

using namespace esphome::alarm_control_panel;

static const char *const TAG = "template.alarm_control_panel";

TemplateAlarmControlPanel::TemplateAlarmControlPanel(){};

void TemplateAlarmControlPanel::add_sensor(binary_sensor::BinarySensor *sensor, bool bypass_when_home) {
  this->sensors_.push_back(sensor);
  if (bypass_when_home) {
    this->bypass_when_home_.push_back(sensor);
  }
};

void TemplateAlarmControlPanel::dump_config() {
  ESP_LOGCONFIG(TAG, "TemplateAlarmControlPanel:");
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

void TemplateAlarmControlPanel::setup() {
  uint8_t value;
  this->pref_ = global_preferences->make_preference<uint8_t>(this->get_object_id_hash());
  if (this->pref_.load(&value)) {
    this->current_state_ = static_cast<alarm_control_panel::AlarmControlPanelState>(value);
  } else {
    this->current_state_ = ACP_STATE_DISARMED;
  }
  this->desired_state_ = this->current_state_;
}

void TemplateAlarmControlPanel::loop() {
  // change from ARMING to ARMED_x after the arming_time_ has passed
  if (this->current_state_ == ACP_STATE_ARMING) {
    auto delay = this->arming_away_time_;
    if (this->desired_state_ == ACP_STATE_ARMED_HOME) {
      delay = this->arming_home_time_;
    }
    if ((millis() - this->last_update_) > delay) {
      this->set_panel_state(this->desired_state_);
    }
    return;
  }
  // change from PENDING to TRIGGERED after the delay_time_ has passed
  if (this->current_state_ == ACP_STATE_PENDING && (millis() - this->last_update_) > this->delay_time_) {
    this->set_panel_state(ACP_STATE_TRIGGERED);
    return;
  }
  auto future_state = this->current_state_;
  // reset triggered if all clear
  if (this->current_state_ == ACP_STATE_TRIGGERED && this->trigger_time_ > 0 &&
      (millis() - this->last_update_) > this->trigger_time_) {
    future_state = this->desired_state_;
  }
  bool trigger = false;
  if (this->is_state_armed(future_state)) {
    // TODO might be better to register change for each sensor in setup...
    for (binary_sensor::BinarySensor *sensor : this->sensors_) {
      if (sensor->state) {
        if (this->current_state_ == ACP_STATE_ARMED_HOME &&
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
    if (this->delay_time_ > 0 && this->current_state_ != ACP_STATE_TRIGGERED) {
      this->set_panel_state(ACP_STATE_PENDING);
    } else {
      this->set_panel_state(ACP_STATE_TRIGGERED);
    }
  } else if (future_state != this->current_state_) {
    ESP_LOGD(TAG, "need to update state...");
    this->set_panel_state(future_state);
  }
}

void TemplateAlarmControlPanel::add_code(const std::string &code) { this->codes_.push_back(code); }

bool TemplateAlarmControlPanel::is_code_valid_(optional<std::string> code) {
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

uint32_t TemplateAlarmControlPanel::get_supported_features() {
  auto features = ACP_FEAT_ARM_AWAY + ACP_FEAT_TRIGGER;
  if (!this->bypass_when_home_.empty()) {
    features += ACP_FEAT_ARM_HOME;
  }
  return features;
}

bool TemplateAlarmControlPanel::get_requires_code() { return !this->codes_.empty(); }

void TemplateAlarmControlPanel::set_requires_code_to_arm(bool code_to_arm) {
  this->requires_code_to_arm_ = code_to_arm;
}

bool TemplateAlarmControlPanel::get_requires_code_to_arm() { return this->requires_code_to_arm_; }

void TemplateAlarmControlPanel::set_arming_away_time(uint32_t time) { this->arming_away_time_ = time * 1000; }

void TemplateAlarmControlPanel::set_arming_home_time(uint32_t time) { this->arming_home_time_ = time * 1000; }

void TemplateAlarmControlPanel::set_delay_time(uint32_t time) { this->delay_time_ = time * 1000; }

void TemplateAlarmControlPanel::set_trigger_time(uint32_t time) { this->trigger_time_ = time * 1000; }

void TemplateAlarmControlPanel::arm_(optional<std::string> code, alarm_control_panel::AlarmControlPanelState state,
                                     uint32_t delay) {
  if (this->current_state_ != ACP_STATE_DISARMED) {
    ESP_LOGW(TAG, "Cannot arm when not disarmed");
    return;
  }
  if (this->requires_code_to_arm_ && !this->is_code_valid_(std::move(code))) {
    ESP_LOGW(TAG, "Not arming code doesn't match");
    return;
  }
  this->desired_state_ = state;
  if (delay > 0) {
    this->set_panel_state(ACP_STATE_ARMING);
  } else {
    this->set_panel_state(state);
  }
}

void TemplateAlarmControlPanel::control(const AlarmControlPanelCall &call) {
  if (call.get_state()) {
    if (call.get_state() == ACP_STATE_ARMED_AWAY) {
      ESP_LOGD(TAG, "arm_away");
      this->arm_(call.get_code(), ACP_STATE_ARMED_AWAY, this->arming_away_time_);
    } else if (call.get_state() == ACP_STATE_ARMED_HOME) {
      ESP_LOGD(TAG, "arm_home");
      this->arm_(call.get_code(), ACP_STATE_ARMED_HOME, this->arming_home_time_);
    } else if (call.get_state() == ACP_STATE_DISARMED) {
      ESP_LOGD(TAG, "disarm");
      if (!this->is_code_valid_(call.get_code())) {
        ESP_LOGW(TAG, "Not disarming code doesn't match");
        return;
      }
      this->desired_state_ = ACP_STATE_DISARMED;
      this->set_panel_state(ACP_STATE_DISARMED);
    } else if (call.get_state() == ACP_STATE_TRIGGERED) {
      ESP_LOGD(TAG, "triggered");
      this->set_panel_state(ACP_STATE_TRIGGERED);
    } else if (call.get_state() == ACP_STATE_PENDING) {
      ESP_LOGD(TAG, "pending");
      this->set_panel_state(ACP_STATE_PENDING);
    } else {
      ESP_LOGE(TAG, "State not yet implemented: %s", this->to_string(*call.get_state()).c_str());
    }
  }
}

}  // namespace template_
}  // namespace esphome
