#include "template_alarm_control_panel.h"
#include <utility>
#include "esphome/components/alarm_control_panel/alarm_control_panel.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace template_ {

using namespace esphome::alarm_control_panel;

static const char *const TAG = "template.alarm_control_panel";

TemplateAlarmControlPanel::TemplateAlarmControlPanel(){};

#ifdef USE_BINARY_SENSOR
void TemplateAlarmControlPanel::add_sensor(binary_sensor::BinarySensor *sensor, uint16_t flags) {
  this->sensor_map_[sensor] = flags;
};
#endif

void TemplateAlarmControlPanel::dump_config() {
  ESP_LOGCONFIG(TAG, "TemplateAlarmControlPanel:");
  ESP_LOGCONFIG(TAG, "  Current State: %s", LOG_STR_ARG(alarm_control_panel_state_to_string(this->current_state_)));
  ESP_LOGCONFIG(TAG, "  Number of Codes: %u", this->codes_.size());
  if (!this->codes_.empty())
    ESP_LOGCONFIG(TAG, "  Requires Code To Arm: %s", YESNO(this->requires_code_to_arm_));
  ESP_LOGCONFIG(TAG, "  Arming Away Time: %" PRIu32 "s", (this->arming_away_time_ / 1000));
  if (this->arming_home_time_ != 0)
    ESP_LOGCONFIG(TAG, "  Arming Home Time: %" PRIu32 "s", (this->arming_home_time_ / 1000));
  if (this->arming_night_time_ != 0)
    ESP_LOGCONFIG(TAG, "  Arming Night Time: %" PRIu32 "s", (this->arming_night_time_ / 1000));
  ESP_LOGCONFIG(TAG, "  Pending Time: %" PRIu32 "s", (this->pending_time_ / 1000));
  ESP_LOGCONFIG(TAG, "  Trigger Time: %" PRIu32 "s", (this->trigger_time_ / 1000));
  ESP_LOGCONFIG(TAG, "  Supported Features: %" PRIu32, this->get_supported_features());
#ifdef USE_BINARY_SENSOR
  for (auto sensor_pair : this->sensor_map_) {
    ESP_LOGCONFIG(TAG, "  Binary Sesnsor:");
    ESP_LOGCONFIG(TAG, "    Name: %s", sensor_pair.first->get_name().c_str());
    ESP_LOGCONFIG(TAG, "    Armed home bypass: %s",
                  TRUEFALSE(sensor_pair.second & BINARY_SENSOR_MODE_BYPASS_ARMED_HOME));
    ESP_LOGCONFIG(TAG, "    Armed night bypass: %s",
                  TRUEFALSE(sensor_pair.second & BINARY_SENSOR_MODE_BYPASS_ARMED_NIGHT));
  }
#endif
}

void TemplateAlarmControlPanel::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Template AlarmControlPanel '%s'...", this->name_.c_str());
  switch (this->restore_mode_) {
    case ALARM_CONTROL_PANEL_ALWAYS_DISARMED:
      this->current_state_ = ACP_STATE_DISARMED;
      break;
    case ALARM_CONTROL_PANEL_RESTORE_DEFAULT_DISARMED: {
      uint8_t value;
      this->pref_ = global_preferences->make_preference<uint8_t>(this->get_object_id_hash());
      if (this->pref_.load(&value)) {
        this->current_state_ = static_cast<alarm_control_panel::AlarmControlPanelState>(value);
      } else {
        this->current_state_ = ACP_STATE_DISARMED;
      }
      break;
    }
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
    if (this->desired_state_ == ACP_STATE_ARMED_NIGHT) {
      delay = this->arming_night_time_;
    }
    if ((millis() - this->last_update_) > delay) {
      this->publish_state(this->desired_state_);
    }
    return;
  }
  // change from PENDING to TRIGGERED after the delay_time_ has passed
  if (this->current_state_ == ACP_STATE_PENDING && (millis() - this->last_update_) > this->pending_time_) {
    this->publish_state(ACP_STATE_TRIGGERED);
    return;
  }
  auto future_state = this->current_state_;
  // reset triggered if all clear
  if (this->current_state_ == ACP_STATE_TRIGGERED && this->trigger_time_ > 0 &&
      (millis() - this->last_update_) > this->trigger_time_) {
    future_state = this->desired_state_;
  }
  bool trigger = false;
#ifdef USE_BINARY_SENSOR
  if (this->is_state_armed(future_state)) {
    // TODO might be better to register change for each sensor in setup...
    for (auto sensor_pair : this->sensor_map_) {
      if (sensor_pair.first->state) {
        if (this->current_state_ == ACP_STATE_ARMED_HOME &&
            (sensor_pair.second & BINARY_SENSOR_MODE_BYPASS_ARMED_HOME)) {
          continue;
        }
        if (this->current_state_ == ACP_STATE_ARMED_NIGHT &&
            (sensor_pair.second & BINARY_SENSOR_MODE_BYPASS_ARMED_NIGHT)) {
          continue;
        }
        trigger = true;
        break;
      }
    }
  }
#endif
  if (trigger) {
    if (this->pending_time_ > 0 && this->current_state_ != ACP_STATE_TRIGGERED) {
      this->publish_state(ACP_STATE_PENDING);
    } else {
      this->publish_state(ACP_STATE_TRIGGERED);
    }
  } else if (future_state != this->current_state_) {
    this->publish_state(future_state);
  }
}

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

uint32_t TemplateAlarmControlPanel::get_supported_features() const {
  uint32_t features = ACP_FEAT_ARM_AWAY | ACP_FEAT_TRIGGER;
  if (this->supports_arm_home_) {
    features |= ACP_FEAT_ARM_HOME;
  }
  if (this->supports_arm_night_) {
    features |= ACP_FEAT_ARM_NIGHT;
  }
  return features;
}

bool TemplateAlarmControlPanel::get_requires_code() const { return !this->codes_.empty(); }

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
    this->publish_state(ACP_STATE_ARMING);
  } else {
    this->publish_state(state);
  }
}

void TemplateAlarmControlPanel::control(const AlarmControlPanelCall &call) {
  if (call.get_state()) {
    if (call.get_state() == ACP_STATE_ARMED_AWAY) {
      this->arm_(call.get_code(), ACP_STATE_ARMED_AWAY, this->arming_away_time_);
    } else if (call.get_state() == ACP_STATE_ARMED_HOME) {
      this->arm_(call.get_code(), ACP_STATE_ARMED_HOME, this->arming_home_time_);
    } else if (call.get_state() == ACP_STATE_ARMED_NIGHT) {
      this->arm_(call.get_code(), ACP_STATE_ARMED_NIGHT, this->arming_night_time_);
    } else if (call.get_state() == ACP_STATE_DISARMED) {
      if (!this->is_code_valid_(call.get_code())) {
        ESP_LOGW(TAG, "Not disarming code doesn't match");
        return;
      }
      this->desired_state_ = ACP_STATE_DISARMED;
      this->publish_state(ACP_STATE_DISARMED);
    } else if (call.get_state() == ACP_STATE_TRIGGERED) {
      this->publish_state(ACP_STATE_TRIGGERED);
    } else if (call.get_state() == ACP_STATE_PENDING) {
      this->publish_state(ACP_STATE_PENDING);
    } else {
      ESP_LOGE(TAG, "State not yet implemented: %s",
               LOG_STR_ARG(alarm_control_panel_state_to_string(*call.get_state())));
    }
  }
}

}  // namespace template_
}  // namespace esphome
