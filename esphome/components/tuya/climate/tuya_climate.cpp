#include "esphome/core/log.h"
#include "tuya_climate.h"

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya.climate";

void TuyaClimate::setup() {
  if (this->switch_id_.has_value()) {
    this->parent_->register_listener(*this->switch_id_, [this](const TuyaDatapoint &datapoint) {
      ESP_LOGV(TAG, "MCU reported switch is: %s", ONOFF(datapoint.value_bool));
      this->mode = climate::CLIMATE_MODE_OFF;
      if (datapoint.value_bool) {
        if (this->supports_heat_ && this->supports_cool_) {
          this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        } else if (this->supports_heat_) {
          this->mode = climate::CLIMATE_MODE_HEAT;
        } else if (this->supports_cool_) {
          this->mode = climate::CLIMATE_MODE_COOL;
        }
      }
      this->compute_action_();
      this->publish_state();
    });
  }
  if (this->active_state_id_.has_value()) {
    this->parent_->register_listener(*this->active_state_id_, [this](const TuyaDatapoint &datapoint) {
      ESP_LOGV(TAG, "MCU reported active state is: %u", datapoint.value_enum);
      this->active_state_ = datapoint.value_enum;
      if (this->compute_action_())
        this->publish_state();
    });
  } else {
    if (this->heating_state_pin_ != nullptr) {
      this->heating_state_pin_->setup();
      this->heating_state_ = this->heating_state_pin_->digital_read();
    }
    if (this->cooling_state_pin_ != nullptr) {
      this->cooling_state_pin_->setup();
      this->cooling_state_ = this->cooling_state_pin_->digital_read();
    }
  }
  if (this->target_temperature_id_.has_value()) {
    this->parent_->register_listener(*this->target_temperature_id_, [this](const TuyaDatapoint &datapoint) {
      this->manual_temperature_ = datapoint.value_int * this->target_temperature_multiplier_;
      ESP_LOGV(TAG, "MCU reported manual target temperature is: %.1f", this->manual_temperature_);
      this->compute_target_temperature_();
      this->compute_action_();
      this->publish_state();
    });
  }
  if (this->current_temperature_id_.has_value()) {
    this->parent_->register_listener(*this->current_temperature_id_, [this](const TuyaDatapoint &datapoint) {
      this->current_temperature = datapoint.value_int * this->current_temperature_multiplier_;
      ESP_LOGV(TAG, "MCU reported current temperature is: %.1f", this->current_temperature);
      this->compute_action_();
      this->publish_state();
    });
  }
  if (this->eco_id_.has_value()) {
    this->parent_->register_listener(*this->eco_id_, [this](const TuyaDatapoint &datapoint) {
      this->eco_ = datapoint.value_bool;
      ESP_LOGV(TAG, "MCU reported eco is: %s", ONOFF(this->eco_));
      this->compute_preset_();
      this->compute_target_temperature_();
      this->publish_state();
    });
  }
  if (this->manual_mode_id_.has_value()) {
    this->parent_->register_listener(*this->manual_mode_id_, [this](const TuyaDatapoint &datapoint) {
      this->manual_mode_ = datapoint.value_bool;
      ESP_LOGV(TAG, "MCU reported manual mode is: %s", ONOFF(this->manual_mode_));
      if (this->compute_target_temperature_())
        this->publish_state();
    });
  }
  if (this->schedule_id_.has_value()) {
    this->parent_->register_listener(*this->schedule_id_, [this](const TuyaDatapoint &datapoint) {
      ESP_LOGV(TAG, "MCU reported schedule");
      if (this->compute_target_temperature_())
        this->publish_state();
    });
  }
}

void TuyaClimate::loop() {
  bool state_changed = false;

  // Read heating and cooling state pins, if used
  if (this->heating_state_pin_ != nullptr) {
    bool heating_state = this->heating_state_pin_->digital_read();
    if (heating_state != this->heating_state_) {
      ESP_LOGV(TAG, "Heating state pin changed to: %s", ONOFF(heating_state));
      this->heating_state_ = heating_state;
      state_changed = true;
    }
  }
  if (this->cooling_state_pin_ != nullptr) {
    bool cooling_state = this->cooling_state_pin_->digital_read();
    if (cooling_state != this->cooling_state_) {
      ESP_LOGV(TAG, "Cooling state pin changed to: %s", ONOFF(cooling_state));
      this->cooling_state_ = cooling_state;
      state_changed = true;
    }
  }

#ifdef USE_TIME
  // Recalculate target temperature every minute
  if (this->schedule_id_.has_value() && this->parent_->get_time_id().has_value()) {
    auto time_id = *this->parent_->get_time_id();
    time::ESPTime now = time_id->now();
    if (now.is_valid()) {
      int schedule_check_time = (now.hour * 60 + now.minute);
      if (schedule_check_time != this->last_schedule_check_time_) {
        this->last_schedule_check_time_ = schedule_check_time;
        state_changed |= compute_target_temperature_();
      }
    }
  }
#endif

  if (state_changed) {
    this->compute_action_();
    this->publish_state();
  }
}

void TuyaClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
    const bool switch_state = *call.get_mode() != climate::CLIMATE_MODE_OFF;
    ESP_LOGV(TAG, "Setting switch: %s", ONOFF(switch_state));
    this->parent_->set_boolean_datapoint_value(*this->switch_id_, switch_state);
  }

  if (call.get_target_temperature().has_value()) {
    const float target_temperature = *call.get_target_temperature();
    ESP_LOGV(TAG, "Setting target temperature: %.1f", target_temperature);
    this->parent_->set_integer_datapoint_value(*this->target_temperature_id_,
                                               (int) (target_temperature / this->target_temperature_multiplier_));
  }

  if (call.get_preset().has_value()) {
    const climate::ClimatePreset preset = *call.get_preset();
    if (this->eco_id_.has_value()) {
      const bool eco = preset == climate::CLIMATE_PRESET_ECO;
      ESP_LOGV(TAG, "Setting eco: %s", ONOFF(eco));
      this->parent_->set_boolean_datapoint_value(*this->eco_id_, eco);
    }
  }
}

climate::ClimateTraits TuyaClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_action(true);
  traits.set_supports_current_temperature(this->current_temperature_id_.has_value());
  if (supports_heat_)
    traits.add_supported_mode(climate::CLIMATE_MODE_HEAT);
  if (supports_cool_)
    traits.add_supported_mode(climate::CLIMATE_MODE_COOL);
  if (this->eco_id_.has_value()) {
    traits.add_supported_preset(climate::CLIMATE_PRESET_NONE);
    traits.add_supported_preset(climate::CLIMATE_PRESET_ECO);
  }
  return traits;
}

void TuyaClimate::dump_config() {
  LOG_CLIMATE("", "Tuya Climate", this);
  if (this->switch_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Switch has datapoint ID %u", *this->switch_id_);
  if (this->active_state_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Active state has datapoint ID %u", *this->active_state_id_);
  if (this->target_temperature_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Target Temperature has datapoint ID %u", *this->target_temperature_id_);
  if (this->current_temperature_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Current Temperature has datapoint ID %u", *this->current_temperature_id_);
  LOG_PIN("  Heating State Pin: ", this->heating_state_pin_);
  LOG_PIN("  Cooling State Pin: ", this->cooling_state_pin_);
  if (this->eco_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Eco has datapoint ID %u", *this->eco_id_);
}

bool TuyaClimate::compute_preset_() {
  if (this->eco_) {
    return set_preset_(climate::CLIMATE_PRESET_ECO);
  } else {
    return set_preset_(climate::CLIMATE_PRESET_NONE);
  }
}

bool TuyaClimate::compute_target_temperature_() {
  if (this->eco_ && this->eco_temperature_.has_value()) {
    return set_target_temperature_(*this->eco_temperature_);
  }

#ifdef USE_TIME
  if (!this->manual_mode_ && this->schedule_id_.has_value() && this->parent_->get_time_id().has_value()) {
    auto schedule_datapoint = this->parent_->get_datapoint(*this->schedule_id_);
    auto time_id = *this->parent_->get_time_id();
    auto now = time_id->now();

    if (schedule_datapoint.has_value() && (*schedule_datapoint).type == TuyaDatapointType::RAW && now.is_valid()) {
      auto schedule = (*schedule_datapoint).value_raw;
      auto schedule_size = schedule.size();

      if (schedule_size == 54 || schedule_size == 36) {
        // Default schedule for BHT-3000 (entry format: MM.HH.TT - schedule_size: 54)
        // 00.06.28|00.08.1E|1E.0B.1E|1E.0D.1E|00.11.2C|00.16.1E -- Monday-Friday
        // 00.06.28|00.08.28|1E.0B.28|1E.0D.28|00.11.28|00.16.1E -- Saturday
        // 00.06.28|00.08.28|1E.0B.28|1E.0D.28|00.11.28|00.16.1E -- Sunday

        uint8_t entry = (schedule_size == 54) ? (now.day_of_week == 1 ? 12 : (now.day_of_week == 7 ? 6 : 0))
                                              : (now.day_of_week == 1 || now.day_of_week == 7 ? 6 : 0);

        // Entry format may be MM.HH.TT (little-endian / inverted time) or HH.MM.TT (big-endian / non-inverted time)
        uint8_t hour_offset = this->schedule_time_inverted_ ? 1 : 0;
        uint8_t minute_offset = this->schedule_time_inverted_ ? 0 : 1;

        uint8_t hour = schedule[entry * 3 + hour_offset];
        uint8_t minute = schedule[entry * 3 + minute_offset];
        if (now.hour < hour || (now.hour == hour && now.minute < minute)) {
          // Current time is before the start time of the first entry; use last entry of previous day
          entry = (schedule_size == 54) ? (now.day_of_week == 2 ? 17 : (now.day_of_week == 1 ? 11 : 5))
                                        : (now.day_of_week == 2 || now.day_of_week == 1 ? 11 : 5);
          // Search for active entry of current day
          for (int i = 1; i <= 5; i++) {
            hour = schedule[(entry + 1) * 3 + hour_offset];
            minute = schedule[(entry + 1) * 3 + minute_offset];
            if (now.hour > hour || (now.hour == hour && now.minute >= minute)) {
              entry++;
            } else {
              break;
            }
          }
        }

        return set_target_temperature_(schedule[entry * 3 + 2] * this->target_temperature_multiplier_);
      } else {
        ESP_LOGV(TAG, "Unknown schedule size of %u bytes", schedule_size);
      }
    }
  }
#endif

  return set_target_temperature_(this->manual_temperature_);
}

bool TuyaClimate::set_target_temperature_(float target_temperature) {
  if (target_temperature == this->target_temperature)
    return false;

  this->target_temperature = target_temperature;
  return true;
}

bool TuyaClimate::compute_action_() {
  if (isnan(this->current_temperature) || isnan(this->target_temperature)) {
    // if any control parameters are nan, go to OFF action (not IDLE!)
    return this->set_action_(climate::CLIMATE_ACTION_OFF);
  }

  if (this->mode == climate::CLIMATE_MODE_OFF) {
    return this->set_action_(climate::CLIMATE_ACTION_OFF);
  }

  if (this->active_state_id_.has_value()) {
    // Use state from MCU datapoint
    if (this->supports_heat_ && this->active_state_heating_value_.has_value() &&
        this->active_state_ == this->active_state_heating_value_) {
      return this->set_action_(climate::CLIMATE_ACTION_HEATING);
    } else if (this->supports_cool_ && this->active_state_cooling_value_.has_value() &&
               this->active_state_ == this->active_state_cooling_value_) {
      return this->set_action_(climate::CLIMATE_ACTION_COOLING);
    }
  } else if (this->heating_state_pin_ != nullptr || this->cooling_state_pin_ != nullptr) {
    // Use state from input pins
    if (this->heating_state_) {
      return this->set_action_(climate::CLIMATE_ACTION_HEATING);
    } else if (this->cooling_state_) {
      return this->set_action_(climate::CLIMATE_ACTION_COOLING);
    }
  } else {
    // Fallback to active state calc based on temp and hysteresis
    const float temp_diff = this->target_temperature - this->current_temperature;
    if (std::abs(temp_diff) > this->hysteresis_) {
      if (this->supports_heat_ && temp_diff > 0) {
        return this->set_action_(climate::CLIMATE_ACTION_HEATING);
      } else if (this->supports_cool_ && temp_diff < 0) {
        return this->set_action_(climate::CLIMATE_ACTION_COOLING);
      }
    }
  }

  return this->set_action_(climate::CLIMATE_ACTION_IDLE);
}

bool TuyaClimate::set_action_(climate::ClimateAction action) {
  // For now this just sets the current action but could include triggers later
  if (action == this->action)
    return false;

  this->action = action;
  return true;
}

}  // namespace tuya
}  // namespace esphome
