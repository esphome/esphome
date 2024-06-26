#include "tuya_climate.h"
#include "esphome/core/log.h"

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
      this->compute_state_();
      this->publish_state();
    });
  }
  if (this->heating_state_pin_ != nullptr) {
    this->heating_state_pin_->setup();
    this->heating_state_ = this->heating_state_pin_->digital_read();
  }
  if (this->cooling_state_pin_ != nullptr) {
    this->cooling_state_pin_->setup();
    this->cooling_state_ = this->cooling_state_pin_->digital_read();
  }
  if (this->active_state_id_.has_value()) {
    this->parent_->register_listener(*this->active_state_id_, [this](const TuyaDatapoint &datapoint) {
      ESP_LOGV(TAG, "MCU reported active state is: %u", datapoint.value_enum);
      this->active_state_ = datapoint.value_enum;
      this->compute_state_();
      this->publish_state();
    });
  }
  if (this->target_temperature_id_.has_value()) {
    this->parent_->register_listener(*this->target_temperature_id_, [this](const TuyaDatapoint &datapoint) {
      this->manual_temperature_ = datapoint.value_int * this->target_temperature_multiplier_;
      if (this->reports_fahrenheit_) {
        this->manual_temperature_ = (this->manual_temperature_ - 32) * 5 / 9;
      }

      ESP_LOGV(TAG, "MCU reported manual target temperature is: %.1f", this->manual_temperature_);
      this->compute_target_temperature_();
      this->compute_state_();
      this->publish_state();
    });
  }
  if (this->current_temperature_id_.has_value()) {
    this->parent_->register_listener(*this->current_temperature_id_, [this](const TuyaDatapoint &datapoint) {
      this->current_temperature = datapoint.value_int * this->current_temperature_multiplier_;
      if (this->reports_fahrenheit_) {
        this->current_temperature = (this->current_temperature - 32) * 5 / 9;
      }

      ESP_LOGV(TAG, "MCU reported current temperature is: %.1f", this->current_temperature);
      this->compute_state_();
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
  if (this->sleep_id_.has_value()) {
    this->parent_->register_listener(*this->sleep_id_, [this](const TuyaDatapoint &datapoint) {
      this->sleep_ = datapoint.value_bool;
      ESP_LOGV(TAG, "MCU reported sleep is: %s", ONOFF(this->sleep_));
      this->compute_preset_();
      this->compute_target_temperature_();
      this->publish_state();
    });
  }
  if (this->swing_vertical_id_.has_value()) {
    this->parent_->register_listener(*this->swing_vertical_id_, [this](const TuyaDatapoint &datapoint) {
      this->swing_vertical_ = datapoint.value_bool;
      ESP_LOGV(TAG, "MCU reported vertical swing is: %s", ONOFF(datapoint.value_bool));
      this->compute_swingmode_();
      this->publish_state();
    });
  }

  if (this->swing_horizontal_id_.has_value()) {
    this->parent_->register_listener(*this->swing_horizontal_id_, [this](const TuyaDatapoint &datapoint) {
      this->swing_horizontal_ = datapoint.value_bool;
      ESP_LOGV(TAG, "MCU reported horizontal swing is: %s", ONOFF(datapoint.value_bool));
      this->compute_swingmode_();
      this->publish_state();
    });
  }

  if (this->fan_speed_id_.has_value()) {
    this->parent_->register_listener(*this->fan_speed_id_, [this](const TuyaDatapoint &datapoint) {
      ESP_LOGV(TAG, "MCU reported Fan Speed Mode is: %u", datapoint.value_enum);
      this->fan_state_ = datapoint.value_enum;
      this->compute_fanmode_();
      this->publish_state();
    });
  }
}

void TuyaClimate::loop() {
  bool state_changed = false;
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

  if (state_changed) {
    this->compute_state_();
    this->publish_state();
  }
}

void TuyaClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
    const bool switch_state = *call.get_mode() != climate::CLIMATE_MODE_OFF;
    ESP_LOGV(TAG, "Setting switch: %s", ONOFF(switch_state));
    this->parent_->set_boolean_datapoint_value(*this->switch_id_, switch_state);
    const climate::ClimateMode new_mode = *call.get_mode();

    if (this->active_state_id_.has_value()) {
      if (new_mode == climate::CLIMATE_MODE_HEAT && this->supports_heat_) {
        this->parent_->set_enum_datapoint_value(*this->active_state_id_, *this->active_state_heating_value_);
      } else if (new_mode == climate::CLIMATE_MODE_COOL && this->supports_cool_) {
        this->parent_->set_enum_datapoint_value(*this->active_state_id_, *this->active_state_cooling_value_);
      } else if (new_mode == climate::CLIMATE_MODE_DRY && this->active_state_drying_value_.has_value()) {
        this->parent_->set_enum_datapoint_value(*this->active_state_id_, *this->active_state_drying_value_);
      } else if (new_mode == climate::CLIMATE_MODE_FAN_ONLY && this->active_state_fanonly_value_.has_value()) {
        this->parent_->set_enum_datapoint_value(*this->active_state_id_, *this->active_state_fanonly_value_);
      }
    } else {
      ESP_LOGW(TAG, "Active state (mode) datapoint not configured");
    }
  }

  control_swing_mode_(call);
  control_fan_mode_(call);

  if (call.get_target_temperature().has_value()) {
    float target_temperature = *call.get_target_temperature();
    if (this->reports_fahrenheit_)
      target_temperature = (target_temperature * 9 / 5) + 32;

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
    if (this->sleep_id_.has_value()) {
      const bool sleep = preset == climate::CLIMATE_PRESET_SLEEP;
      ESP_LOGV(TAG, "Setting sleep: %s", ONOFF(sleep));
      this->parent_->set_boolean_datapoint_value(*this->sleep_id_, sleep);
    }
  }
}

void TuyaClimate::control_swing_mode_(const climate::ClimateCall &call) {
  bool vertical_swing_changed = false;
  bool horizontal_swing_changed = false;

  if (call.get_swing_mode().has_value()) {
    const auto swing_mode = *call.get_swing_mode();

    switch (swing_mode) {
      case climate::CLIMATE_SWING_OFF:
        if (swing_vertical_ || swing_horizontal_) {
          this->swing_vertical_ = false;
          this->swing_horizontal_ = false;
          vertical_swing_changed = true;
          horizontal_swing_changed = true;
        }
        break;

      case climate::CLIMATE_SWING_BOTH:
        if (!swing_vertical_ || !swing_horizontal_) {
          this->swing_vertical_ = true;
          this->swing_horizontal_ = true;
          vertical_swing_changed = true;
          horizontal_swing_changed = true;
        }
        break;

      case climate::CLIMATE_SWING_VERTICAL:
        if (!swing_vertical_ || swing_horizontal_) {
          this->swing_vertical_ = true;
          this->swing_horizontal_ = false;
          vertical_swing_changed = true;
          horizontal_swing_changed = true;
        }
        break;

      case climate::CLIMATE_SWING_HORIZONTAL:
        if (swing_vertical_ || !swing_horizontal_) {
          this->swing_vertical_ = false;
          this->swing_horizontal_ = true;
          vertical_swing_changed = true;
          horizontal_swing_changed = true;
        }
        break;

      default:
        break;
    }
  }

  if (vertical_swing_changed && this->swing_vertical_id_.has_value()) {
    ESP_LOGV(TAG, "Setting vertical swing: %s", ONOFF(swing_vertical_));
    this->parent_->set_boolean_datapoint_value(*this->swing_vertical_id_, swing_vertical_);
  }

  if (horizontal_swing_changed && this->swing_horizontal_id_.has_value()) {
    ESP_LOGV(TAG, "Setting horizontal swing: %s", ONOFF(swing_horizontal_));
    this->parent_->set_boolean_datapoint_value(*this->swing_horizontal_id_, swing_horizontal_);
  }

  // Publish the state after updating the swing mode
  this->publish_state();
}

void TuyaClimate::control_fan_mode_(const climate::ClimateCall &call) {
  if (call.get_fan_mode().has_value()) {
    climate::ClimateFanMode fan_mode = *call.get_fan_mode();

    uint8_t tuya_fan_speed;
    switch (fan_mode) {
      case climate::CLIMATE_FAN_LOW:
        tuya_fan_speed = *fan_speed_low_value_;
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        tuya_fan_speed = *fan_speed_medium_value_;
        break;
      case climate::CLIMATE_FAN_MIDDLE:
        tuya_fan_speed = *fan_speed_middle_value_;
        break;
      case climate::CLIMATE_FAN_HIGH:
        tuya_fan_speed = *fan_speed_high_value_;
        break;
      case climate::CLIMATE_FAN_AUTO:
        tuya_fan_speed = *fan_speed_auto_value_;
        break;
      default:
        tuya_fan_speed = 0;
        break;
    }

    if (this->fan_speed_id_.has_value()) {
      this->parent_->set_enum_datapoint_value(*this->fan_speed_id_, tuya_fan_speed);
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
  if (this->active_state_drying_value_.has_value())
    traits.add_supported_mode(climate::CLIMATE_MODE_DRY);
  if (this->active_state_fanonly_value_.has_value())
    traits.add_supported_mode(climate::CLIMATE_MODE_FAN_ONLY);
  if (this->eco_id_.has_value()) {
    traits.add_supported_preset(climate::CLIMATE_PRESET_ECO);
  }
  if (this->sleep_id_.has_value()) {
    traits.add_supported_preset(climate::CLIMATE_PRESET_SLEEP);
  }
  if (this->sleep_id_.has_value() || this->eco_id_.has_value()) {
    traits.add_supported_preset(climate::CLIMATE_PRESET_NONE);
  }
  if (this->swing_vertical_id_.has_value() && this->swing_horizontal_id_.has_value()) {
    std::set<climate::ClimateSwingMode> supported_swing_modes = {
        climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_BOTH, climate::CLIMATE_SWING_VERTICAL,
        climate::CLIMATE_SWING_HORIZONTAL};
    traits.set_supported_swing_modes(std::move(supported_swing_modes));
  } else if (this->swing_vertical_id_.has_value()) {
    std::set<climate::ClimateSwingMode> supported_swing_modes = {climate::CLIMATE_SWING_OFF,
                                                                 climate::CLIMATE_SWING_VERTICAL};
    traits.set_supported_swing_modes(std::move(supported_swing_modes));
  } else if (this->swing_horizontal_id_.has_value()) {
    std::set<climate::ClimateSwingMode> supported_swing_modes = {climate::CLIMATE_SWING_OFF,
                                                                 climate::CLIMATE_SWING_HORIZONTAL};
    traits.set_supported_swing_modes(std::move(supported_swing_modes));
  }

  if (fan_speed_id_) {
    if (fan_speed_low_value_)
      traits.add_supported_fan_mode(climate::CLIMATE_FAN_LOW);
    if (fan_speed_medium_value_)
      traits.add_supported_fan_mode(climate::CLIMATE_FAN_MEDIUM);
    if (fan_speed_middle_value_)
      traits.add_supported_fan_mode(climate::CLIMATE_FAN_MIDDLE);
    if (fan_speed_high_value_)
      traits.add_supported_fan_mode(climate::CLIMATE_FAN_HIGH);
    if (fan_speed_auto_value_)
      traits.add_supported_fan_mode(climate::CLIMATE_FAN_AUTO);
  }
  return traits;
}

void TuyaClimate::dump_config() {
  LOG_CLIMATE("", "Tuya Climate", this);
  if (this->switch_id_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Switch has datapoint ID %u", *this->switch_id_);
  }
  if (this->active_state_id_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Active state has datapoint ID %u", *this->active_state_id_);
  }
  if (this->target_temperature_id_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Target Temperature has datapoint ID %u", *this->target_temperature_id_);
  }
  if (this->current_temperature_id_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Current Temperature has datapoint ID %u", *this->current_temperature_id_);
  }
  LOG_PIN("  Heating State Pin: ", this->heating_state_pin_);
  LOG_PIN("  Cooling State Pin: ", this->cooling_state_pin_);
  if (this->eco_id_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Eco has datapoint ID %u", *this->eco_id_);
  }
  if (this->sleep_id_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Sleep has datapoint ID %u", *this->sleep_id_);
  }
  if (this->swing_vertical_id_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Swing Vertical has datapoint ID %u", *this->swing_vertical_id_);
  }
  if (this->swing_horizontal_id_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Swing Horizontal has datapoint ID %u", *this->swing_horizontal_id_);
  }
}

void TuyaClimate::compute_preset_() {
  if (this->eco_) {
    this->preset = climate::CLIMATE_PRESET_ECO;
  } else if (this->sleep_) {
    this->preset = climate::CLIMATE_PRESET_SLEEP;
  } else {
    this->preset = climate::CLIMATE_PRESET_NONE;
  }
}

void TuyaClimate::compute_swingmode_() {
  if (this->swing_vertical_ && this->swing_horizontal_) {
    this->swing_mode = climate::CLIMATE_SWING_BOTH;
  } else if (this->swing_vertical_) {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else if (this->swing_horizontal_) {
    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  } else {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }
}

void TuyaClimate::compute_fanmode_() {
  if (this->fan_speed_id_.has_value()) {
    // Use state from MCU datapoint
    if (this->fan_speed_auto_value_.has_value() && this->fan_state_ == this->fan_speed_auto_value_) {
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
    } else if (this->fan_speed_high_value_.has_value() && this->fan_state_ == this->fan_speed_high_value_) {
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
    } else if (this->fan_speed_medium_value_.has_value() && this->fan_state_ == this->fan_speed_medium_value_) {
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
    } else if (this->fan_speed_middle_value_.has_value() && this->fan_state_ == this->fan_speed_middle_value_) {
      this->fan_mode = climate::CLIMATE_FAN_MIDDLE;
    } else if (this->fan_speed_low_value_.has_value() && this->fan_state_ == this->fan_speed_low_value_) {
      this->fan_mode = climate::CLIMATE_FAN_LOW;
    }
  }
}

void TuyaClimate::compute_target_temperature_() {
  if (this->eco_ && this->eco_temperature_.has_value()) {
    this->target_temperature = *this->eco_temperature_;
  } else {
    this->target_temperature = this->manual_temperature_;
  }
}

void TuyaClimate::compute_state_() {
  if (std::isnan(this->current_temperature) || std::isnan(this->target_temperature)) {
    // if any control parameters are nan, go to OFF action (not IDLE!)
    this->switch_to_action_(climate::CLIMATE_ACTION_OFF);
    return;
  }

  if (this->mode == climate::CLIMATE_MODE_OFF) {
    this->switch_to_action_(climate::CLIMATE_ACTION_OFF);
    return;
  }

  climate::ClimateAction target_action = climate::CLIMATE_ACTION_IDLE;
  if (this->heating_state_pin_ != nullptr || this->cooling_state_pin_ != nullptr) {
    // Use state from input pins
    if (this->heating_state_) {
      target_action = climate::CLIMATE_ACTION_HEATING;
      this->mode = climate::CLIMATE_MODE_HEAT;
    } else if (this->cooling_state_) {
      target_action = climate::CLIMATE_ACTION_COOLING;
      this->mode = climate::CLIMATE_MODE_COOL;
    }
    if (this->active_state_id_.has_value()) {
      // Both are available, use MCU datapoint as mode
      if (this->supports_heat_ && this->active_state_heating_value_.has_value() &&
          this->active_state_ == this->active_state_heating_value_) {
        this->mode = climate::CLIMATE_MODE_HEAT;
      } else if (this->supports_cool_ && this->active_state_cooling_value_.has_value() &&
                 this->active_state_ == this->active_state_cooling_value_) {
        this->mode = climate::CLIMATE_MODE_COOL;
      } else if (this->active_state_drying_value_.has_value() &&
                 this->active_state_ == this->active_state_drying_value_) {
        this->mode = climate::CLIMATE_MODE_DRY;
      } else if (this->active_state_fanonly_value_.has_value() &&
                 this->active_state_ == this->active_state_fanonly_value_) {
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
      }
    }
  } else if (this->active_state_id_.has_value()) {
    // Use state from MCU datapoint
    if (this->supports_heat_ && this->active_state_heating_value_.has_value() &&
        this->active_state_ == this->active_state_heating_value_) {
      target_action = climate::CLIMATE_ACTION_HEATING;
      this->mode = climate::CLIMATE_MODE_HEAT;
    } else if (this->supports_cool_ && this->active_state_cooling_value_.has_value() &&
               this->active_state_ == this->active_state_cooling_value_) {
      target_action = climate::CLIMATE_ACTION_COOLING;
      this->mode = climate::CLIMATE_MODE_COOL;
    } else if (this->active_state_drying_value_.has_value() &&
               this->active_state_ == this->active_state_drying_value_) {
      target_action = climate::CLIMATE_ACTION_DRYING;
      this->mode = climate::CLIMATE_MODE_DRY;
    } else if (this->active_state_fanonly_value_.has_value() &&
               this->active_state_ == this->active_state_fanonly_value_) {
      target_action = climate::CLIMATE_ACTION_FAN;
      this->mode = climate::CLIMATE_MODE_FAN_ONLY;
    }
  } else {
    // Fallback to active state calc based on temp and hysteresis
    const float temp_diff = this->target_temperature - this->current_temperature;
    if (std::abs(temp_diff) > this->hysteresis_) {
      if (this->supports_heat_ && temp_diff > 0) {
        target_action = climate::CLIMATE_ACTION_HEATING;
        this->mode = climate::CLIMATE_MODE_HEAT;
      } else if (this->supports_cool_ && temp_diff < 0) {
        target_action = climate::CLIMATE_ACTION_COOLING;
        this->mode = climate::CLIMATE_MODE_COOL;
      }
    }
  }

  this->switch_to_action_(target_action);
}

void TuyaClimate::switch_to_action_(climate::ClimateAction action) {
  // For now this just sets the current action but could include triggers later
  this->action = action;
}

}  // namespace tuya
}  // namespace esphome
