#include "esphome/core/log.h"
#include "tuya_climate.h"

namespace esphome {
namespace tuya {

static const char *TAG = "tuya.climate";

void TuyaClimate::setup() {
  if (this->switch_id_.has_value()) {
    this->parent_->register_listener(*this->switch_id_, [this](TuyaDatapoint datapoint) {
      if (datapoint.value_bool) {
        this->mode = climate::CLIMATE_MODE_HEAT;
      } else {
        this->mode = climate::CLIMATE_MODE_OFF;
      }
      this->compute_state_();
      this->publish_state();
      ESP_LOGD(TAG, "MCU reported switch is: %s", ONOFF(datapoint.value_bool));
    });
  }
  if (this->target_temperature_id_.has_value()) {
    this->parent_->register_listener(*this->target_temperature_id_, [this](TuyaDatapoint datapoint) {
      this->target_temperature = datapoint.value_int * this->target_temperature_multiplier_;
      this->compute_state_();
      this->publish_state();
      ESP_LOGD(TAG, "MCU reported target temperature is: %.1f", this->target_temperature);
    });
  }
  if (this->current_temperature_id_.has_value()) {
    this->parent_->register_listener(*this->current_temperature_id_, [this](TuyaDatapoint datapoint) {
      this->current_temperature = datapoint.value_int * this->current_temperature_multiplier_;
      this->compute_state_();
      this->publish_state();
      ESP_LOGD(TAG, "MCU reported current temperature is: %.1f", this->current_temperature);
    });
  }
}

void TuyaClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
    this->mode = *call.get_mode();

    TuyaDatapoint datapoint{};
    datapoint.id = *this->switch_id_;
    datapoint.type = TuyaDatapointType::BOOLEAN;
    datapoint.value_bool = this->mode != climate::CLIMATE_MODE_OFF;
    this->parent_->set_datapoint_value(datapoint);
    ESP_LOGD(TAG, "Setting switch: %s", ONOFF(datapoint.value_bool));
  }

  if (call.get_target_temperature().has_value()) {
    this->target_temperature = *call.get_target_temperature();

    TuyaDatapoint datapoint{};
    datapoint.id = *this->target_temperature_id_;
    datapoint.type = TuyaDatapointType::INTEGER;
    datapoint.value_int = (int) (this->target_temperature / this->target_temperature_multiplier_);
    this->parent_->set_datapoint_value(datapoint);
    ESP_LOGD(TAG, "Setting target temperature: %.1f", this->target_temperature);
  }

  this->compute_state_();
  this->publish_state();
}

climate::ClimateTraits TuyaClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(this->current_temperature_id_.has_value());
  traits.set_supports_heat_mode(true);
  traits.set_supports_action(true);
  return traits;
}

void TuyaClimate::dump_config() {
  LOG_CLIMATE("", "Tuya Climate", this);
  if (this->switch_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Switch has datapoint ID %u", *this->switch_id_);
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

  const bool too_cold = this->current_temperature < this->target_temperature - 1;
  const bool too_hot = this->current_temperature > this->target_temperature + 1;
  const bool on_target = this->current_temperature == this->target_temperature;

  climate::ClimateAction target_action;
  if (too_cold) {
    // too cold -> show as heating if possible, else idle
    if (this->traits().supports_mode(climate::CLIMATE_MODE_HEAT)) {
      target_action = climate::CLIMATE_ACTION_HEATING;
    } else {
      target_action = climate::CLIMATE_ACTION_IDLE;
    }
  } else if (too_hot) {
    // too hot -> show as cooling if possible, else idle
    if (this->traits().supports_mode(climate::CLIMATE_MODE_COOL)) {
      target_action = climate::CLIMATE_ACTION_COOLING;
    } else {
      target_action = climate::CLIMATE_ACTION_IDLE;
    }
  } else if (on_target) {
    target_action = climate::CLIMATE_ACTION_IDLE;
  } else {
    target_action = this->action;
  }
  this->switch_to_action_(target_action);
}

void TuyaClimate::switch_to_action_(climate::ClimateAction action) {
  // For now this just sets the current action but could include triggers later
  this->action = action;
}

}  // namespace tuya
}  // namespace esphome
