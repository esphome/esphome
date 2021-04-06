#include "esphome/core/log.h"
#include "tuya_climate.h"

namespace esphome {
namespace tuya {

static const char *TAG = "tuya.climate";

void TuyaClimate::setup() {
  if (this->switch_id_.has_value()) {
    this->parent_->register_listener(*this->switch_id_, [this](TuyaDatapoint datapoint) {
      ESP_LOGV(TAG, "MCU reported switch is: %s", ONOFF(datapoint.value_bool));
      this->mode = climate::CLIMATE_MODE_OFF;
      if (datapoint.value_bool) {
        if (this->supports_heat_ && this->supports_cool_) {
          this->mode = climate::CLIMATE_MODE_AUTO;
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
  if (this->active_state_id_.has_value()) {
    this->parent_->register_listener(*this->active_state_id_, [this](TuyaDatapoint datapoint) {
      ESP_LOGV(TAG, "MCU reported active state is: %u", datapoint.value_enum);
      this->active_state_ = datapoint.value_enum;
      this->compute_state_();
      this->publish_state();
    });
  }
  if (this->target_temperature_id_.has_value()) {
    this->parent_->register_listener(*this->target_temperature_id_, [this](TuyaDatapoint datapoint) {
      this->target_temperature = datapoint.value_int * this->target_temperature_multiplier_;
      ESP_LOGV(TAG, "MCU reported target temperature is: %.1f", this->target_temperature);
      this->compute_state_();
      this->publish_state();
    });
  }
  if (this->current_temperature_id_.has_value()) {
    this->parent_->register_listener(*this->current_temperature_id_, [this](TuyaDatapoint datapoint) {
      this->current_temperature = datapoint.value_int * this->current_temperature_multiplier_;
      ESP_LOGV(TAG, "MCU reported current temperature is: %.1f", this->current_temperature);
      this->compute_state_();
      this->publish_state();
    });
  }
}

void TuyaClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
    const bool switch_state = *call.get_mode() != climate::CLIMATE_MODE_OFF;
    ESP_LOGV(TAG, "Setting switch: %s", ONOFF(switch_state));
    this->parent_->set_datapoint_value(*this->switch_id_, switch_state);
  }

  if (call.get_target_temperature().has_value()) {
    const float target_temperature = *call.get_target_temperature();
    ESP_LOGV(TAG, "Setting target temperature: %.1f", target_temperature);
    this->parent_->set_datapoint_value(*this->target_temperature_id_,
                                       (int) (target_temperature / this->target_temperature_multiplier_));
  }
}

climate::ClimateTraits TuyaClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(this->current_temperature_id_.has_value());
  traits.set_supports_heat_mode(this->supports_heat_);
  traits.set_supports_cool_mode(this->supports_cool_);
  traits.set_supports_action(true);
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
}

void TuyaClimate::compute_state_() {
  if (isnan(this->current_temperature) || isnan(this->target_temperature)) {
    // if any control parameters are nan, go to OFF action (not IDLE!)
    this->switch_to_action_(climate::CLIMATE_ACTION_OFF);
    return;
  }

  if (this->mode == climate::CLIMATE_MODE_OFF) {
    this->switch_to_action_(climate::CLIMATE_ACTION_OFF);
    return;
  }

  climate::ClimateAction target_action = climate::CLIMATE_ACTION_IDLE;
  if (this->active_state_id_.has_value()) {
    if (this->supports_heat_ && this->active_state_heating_value_.has_value() &&
        this->active_state_ == this->active_state_heating_value_) {
      target_action = climate::CLIMATE_ACTION_HEATING;
    } else if (this->supports_cool_ && this->active_state_cooling_value_.has_value() &&
               this->active_state_ == this->active_state_cooling_value_) {
      target_action = climate::CLIMATE_ACTION_COOLING;
    }
  } else {
    // Fallback to active state calc based on temp and hysteresis
    const float temp_diff = this->target_temperature - this->current_temperature;
    if (std::abs(temp_diff) > this->hysteresis_) {
      if (this->supports_heat_ && temp_diff > 0) {
        target_action = climate::CLIMATE_ACTION_HEATING;
      } else if (this->supports_cool_ && temp_diff < 0) {
        target_action = climate::CLIMATE_ACTION_COOLING;
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
