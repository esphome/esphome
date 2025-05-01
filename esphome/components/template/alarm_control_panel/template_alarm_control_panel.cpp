
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
void TemplateAlarmControlPanel::add_sensor(binary_sensor::BinarySensor *sensor, uint16_t flags, AlarmSensorType type) {
  // Save the flags and type. Assign a store index for the per sensor data type.
  SensorDataStore sd;
  sd.last_chime_state = false;
  this->sensor_map_[sensor].flags = flags;
  this->sensor_map_[sensor].type = type;
  this->sensor_data_.push_back(sd);
  this->sensor_map_[sensor].store_index = this->next_store_index_++;
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
  for (auto sensor_info : this->sensor_map_) {
    ESP_LOGCONFIG(TAG, "  Binary Sensor:");
    ESP_LOGCONFIG(TAG, "    Name: %s", sensor_info.first->get_name().c_str());
    ESP_LOGCONFIG(TAG, "    Armed home bypass: %s",
                  TRUEFALSE(sensor_info.second.flags & BINARY_SENSOR_MODE_BYPASS_ARMED_HOME));
    ESP_LOGCONFIG(TAG, "    Armed night bypass: %s",
                  TRUEFALSE(sensor_info.second.flags & BINARY_SENSOR_MODE_BYPASS_ARMED_NIGHT));
    ESP_LOGCONFIG(TAG, "    Chime mode: %s", TRUEFALSE(sensor_info.second.flags & BINARY_SENSOR_MODE_CHIME));
    const char *sensor_type;
    switch (sensor_info.second.type) {
      case ALARM_SENSOR_TYPE_INSTANT:
        sensor_type = "instant";
        break;
      case ALARM_SENSOR_TYPE_DELAYED_FOLLOWER:
        sensor_type = "delayed_follower";
        break;
      case ALARM_SENSOR_TYPE_INSTANT_ALWAYS:
        sensor_type = "instant_always";
        break;
      case ALARM_SENSOR_TYPE_DELAYED:
      default:
        sensor_type = "delayed";
    }
    ESP_LOGCONFIG(TAG, "    Sensor type: %s", sensor_type);
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

  bool delayed_sensor_not_ready = false;
  bool instant_sensor_not_ready = false;

#ifdef USE_BINARY_SENSOR
  // Test all of the sensors in the list regardless of the alarm panel state
  for (auto sensor_info : this->sensor_map_) {
    // Check for chime zones
    if ((sensor_info.second.flags & BINARY_SENSOR_MODE_CHIME)) {
      // Look for the transition from closed to open
      if ((!this->sensor_data_[sensor_info.second.store_index].last_chime_state) && (sensor_info.first->state)) {
        // Must be disarmed to chime
        if (this->current_state_ == ACP_STATE_DISARMED) {
          this->chime_callback_.call();
        }
      }
      // Record the sensor state change
      this->sensor_data_[sensor_info.second.store_index].last_chime_state = sensor_info.first->state;
    }
    // Check for triggered sensors
    if (sensor_info.first->state) {  // Sensor triggered?
      // Skip if bypass armed home
      if (this->current_state_ == ACP_STATE_ARMED_HOME &&
          (sensor_info.second.flags & BINARY_SENSOR_MODE_BYPASS_ARMED_HOME)) {
        continue;
      }
      // Skip if bypass armed night
      if (this->current_state_ == ACP_STATE_ARMED_NIGHT &&
          (sensor_info.second.flags & BINARY_SENSOR_MODE_BYPASS_ARMED_NIGHT)) {
        continue;
      }

      switch (sensor_info.second.type) {
        case ALARM_SENSOR_TYPE_INSTANT:
          instant_sensor_not_ready = true;
          break;
        case ALARM_SENSOR_TYPE_INSTANT_ALWAYS:
          instant_sensor_not_ready = true;
          future_state = ACP_STATE_TRIGGERED;
          break;
        case ALARM_SENSOR_TYPE_DELAYED_FOLLOWER:
          // Look to see if we are in the pending state
          if (this->current_state_ == ACP_STATE_PENDING) {
            delayed_sensor_not_ready = true;
          } else {
            instant_sensor_not_ready = true;
          }
          break;
        case ALARM_SENSOR_TYPE_DELAYED:
        default:
          delayed_sensor_not_ready = true;
      }
    }
  }
  // Update all sensors not ready flag
  this->sensors_ready_ = ((!instant_sensor_not_ready) && (!delayed_sensor_not_ready));

  // Call the ready state change callback if there was a change
  if (this->sensors_ready_ != this->sensors_ready_last_) {
    this->ready_callback_.call();
    this->sensors_ready_last_ = this->sensors_ready_;
  }

#endif
  if (this->is_state_armed(future_state) && (!this->sensors_ready_)) {
    // Instant sensors
    if (instant_sensor_not_ready) {
      this->publish_state(ACP_STATE_TRIGGERED);
    } else if (delayed_sensor_not_ready) {
      // Delayed sensors
      if ((this->pending_time_ > 0) && (this->current_state_ != ACP_STATE_TRIGGERED)) {
        this->publish_state(ACP_STATE_PENDING);
      } else {
        this->publish_state(ACP_STATE_TRIGGERED);
      }
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
