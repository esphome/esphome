
#include "template_alarm_control_panel.h"
#include <utility>
#include "esphome/components/alarm_control_panel/alarm_control_panel.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::template_ {

using namespace esphome::alarm_control_panel;

static const char *const TAG = "template.alarm_control_panel";

TemplateAlarmControlPanel::TemplateAlarmControlPanel(){};

#ifdef USE_BINARY_SENSOR
void TemplateAlarmControlPanel::add_sensor(binary_sensor::BinarySensor *sensor, uint16_t flags, AlarmSensorType type) {
  // Save the flags and type. Assign a store index for the per sensor data type.
  SensorDataStore sd;
  sd.last_chime_state = false;
  AlarmSensor alarm_sensor;
  alarm_sensor.sensor = sensor;
  alarm_sensor.info.flags = flags;
  alarm_sensor.info.type = type;
  alarm_sensor.info.store_index = this->next_store_index_++;
  this->sensors_.push_back(alarm_sensor);
  this->sensor_data_.push_back(sd);
};

static const LogString *sensor_type_to_string(AlarmSensorType type) {
  switch (type) {
    case ALARM_SENSOR_TYPE_INSTANT:
      return LOG_STR("instant");
    case ALARM_SENSOR_TYPE_DELAYED_FOLLOWER:
      return LOG_STR("delayed_follower");
    case ALARM_SENSOR_TYPE_INSTANT_ALWAYS:
      return LOG_STR("instant_always");
    case ALARM_SENSOR_TYPE_DELAYED:
    default:
      return LOG_STR("delayed");
  }
}
#endif

void TemplateAlarmControlPanel::dump_config() {
  ESP_LOGCONFIG(TAG,
                "TemplateAlarmControlPanel:\n"
                "  Current State: %s\n"
                "  Number of Codes: %zu\n"
                "  Requires Code To Arm: %s\n"
                "  Arming Away Time: %" PRIu32 "s\n"
                "  Arming Home Time: %" PRIu32 "s\n"
                "  Arming Night Time: %" PRIu32 "s\n"
                "  Pending Time: %" PRIu32 "s\n"
                "  Trigger Time: %" PRIu32 "s\n"
                "  Supported Features: %" PRIu32,
                LOG_STR_ARG(alarm_control_panel_state_to_string(this->current_state_)), this->codes_.size(),
                YESNO(!this->codes_.empty() && this->requires_code_to_arm_), (this->arming_away_time_ / 1000),
                (this->arming_home_time_ / 1000), (this->arming_night_time_ / 1000), (this->pending_time_ / 1000),
                (this->trigger_time_ / 1000), this->get_supported_features());
#ifdef USE_BINARY_SENSOR
  for (const auto &alarm_sensor : this->sensors_) {
    const uint16_t flags = alarm_sensor.info.flags;
    ESP_LOGCONFIG(TAG,
                  "  Binary Sensor:\n"
                  "    Name: %s\n"
                  "    Type: %s\n"
                  "    Armed home bypass: %s\n"
                  "    Armed night bypass: %s\n"
                  "    Auto bypass: %s\n"
                  "    Chime mode: %s",
                  alarm_sensor.sensor->get_name().c_str(), LOG_STR_ARG(sensor_type_to_string(alarm_sensor.info.type)),
                  TRUEFALSE(flags & BINARY_SENSOR_MODE_BYPASS_ARMED_HOME),
                  TRUEFALSE(flags & BINARY_SENSOR_MODE_BYPASS_ARMED_NIGHT),
                  TRUEFALSE(flags & BINARY_SENSOR_MODE_BYPASS_AUTO), TRUEFALSE(flags & BINARY_SENSOR_MODE_CHIME));
  }
#endif
}

void TemplateAlarmControlPanel::setup() {
  this->current_state_ = ACP_STATE_DISARMED;
  if (this->restore_mode_ == ALARM_CONTROL_PANEL_RESTORE_DEFAULT_DISARMED) {
    uint8_t value;
    this->pref_ = global_preferences->make_preference<uint8_t>(this->get_preference_hash());
    if (this->pref_.load(&value)) {
      this->current_state_ = static_cast<alarm_control_panel::AlarmControlPanelState>(value);
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
      this->bypass_before_arming();
      this->publish_state(this->desired_state_);
    }
    return;
  }
  // change from PENDING to TRIGGERED after the delay_time_ has passed
  if (this->current_state_ == ACP_STATE_PENDING && (millis() - this->last_update_) > this->pending_time_) {
    this->publish_state(ACP_STATE_TRIGGERED);
    return;
  }
  auto next_state = this->current_state_;
  // reset triggered if all clear
  if (this->current_state_ == ACP_STATE_TRIGGERED && this->trigger_time_ > 0 &&
      (millis() - this->last_update_) > this->trigger_time_) {
    next_state = this->desired_state_;
  }

  bool delayed_sensor_faulted = false;
  bool instant_sensor_faulted = false;

#ifdef USE_BINARY_SENSOR
  // Test all of the sensors regardless of the alarm panel state
  for (const auto &alarm_sensor : this->sensors_) {
    const auto &info = alarm_sensor.info;
    auto *sensor = alarm_sensor.sensor;
    // Check for chime zones
    if (info.flags & BINARY_SENSOR_MODE_CHIME) {
      // Look for the transition from closed to open
      if ((!this->sensor_data_[info.store_index].last_chime_state) && (sensor->state)) {
        // Must be disarmed to chime
        if (this->current_state_ == ACP_STATE_DISARMED) {
          this->chime_callback_.call();
        }
      }
      // Record the sensor state change
      this->sensor_data_[info.store_index].last_chime_state = sensor->state;
    }
    // Check for faulted sensors
    if (sensor->state) {
      // Skip if auto bypassed
      if (std::count(this->bypassed_sensor_indicies_.begin(), this->bypassed_sensor_indicies_.end(),
                     info.store_index) == 1) {
        continue;
      }
      // Skip if bypass armed home
      if ((this->current_state_ == ACP_STATE_ARMED_HOME) && (info.flags & BINARY_SENSOR_MODE_BYPASS_ARMED_HOME)) {
        continue;
      }
      // Skip if bypass armed night
      if ((this->current_state_ == ACP_STATE_ARMED_NIGHT) && (info.flags & BINARY_SENSOR_MODE_BYPASS_ARMED_NIGHT)) {
        continue;
      }

      switch (info.type) {
        case ALARM_SENSOR_TYPE_INSTANT_ALWAYS:
          next_state = ACP_STATE_TRIGGERED;
          [[fallthrough]];
        case ALARM_SENSOR_TYPE_INSTANT:
          instant_sensor_faulted = true;
          break;
        case ALARM_SENSOR_TYPE_DELAYED_FOLLOWER:
          // Look to see if we are in the pending state
          if (this->current_state_ == ACP_STATE_PENDING) {
            delayed_sensor_faulted = true;
          } else {
            instant_sensor_faulted = true;
          }
          break;
        case ALARM_SENSOR_TYPE_DELAYED:
        default:
          delayed_sensor_faulted = true;
      }
    }
  }
  // Update all sensors ready flag
  bool sensors_ready = !(instant_sensor_faulted || delayed_sensor_faulted);

  // Call the ready state change callback if there was a change
  if (this->sensors_ready_ != sensors_ready) {
    this->sensors_ready_ = sensors_ready;
    this->ready_callback_.call();
  }

#endif
  if (this->is_state_armed(next_state) && (!this->sensors_ready_)) {
    // Instant sensors
    if (instant_sensor_faulted) {
      this->publish_state(ACP_STATE_TRIGGERED);
    } else if (delayed_sensor_faulted) {
      // Delayed sensors
      if ((this->pending_time_ > 0) && (this->current_state_ != ACP_STATE_TRIGGERED)) {
        this->publish_state(ACP_STATE_PENDING);
      } else {
        this->publish_state(ACP_STATE_TRIGGERED);
      }
    }
  } else if (next_state != this->current_state_) {
    this->publish_state(next_state);
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
    this->bypass_before_arming();
    this->publish_state(state);
  }
}

void TemplateAlarmControlPanel::bypass_before_arming() {
#ifdef USE_BINARY_SENSOR
  for (const auto &alarm_sensor : this->sensors_) {
    // Check for faulted bypass_auto sensors and remove them from monitoring
    if ((alarm_sensor.info.flags & BINARY_SENSOR_MODE_BYPASS_AUTO) && (alarm_sensor.sensor->state)) {
      ESP_LOGW(TAG, "'%s' is faulted and will be automatically bypassed", alarm_sensor.sensor->get_name().c_str());
      this->bypassed_sensor_indicies_.push_back(alarm_sensor.info.store_index);
    }
  }
#endif
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
#ifdef USE_BINARY_SENSOR
      this->bypassed_sensor_indicies_.clear();
#endif
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

}  // namespace esphome::template_
