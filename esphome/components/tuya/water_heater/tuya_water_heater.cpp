#include "tuya_water_heater.h"
#include "esphome/core/log.h"

namespace esphome::tuya {

static const char *const TAG = "tuya.water_heater";

void TuyaWaterHeater::setup() {
  if (this->switch_id_.has_value()) {
    this->parent_->register_listener(*this->switch_id_, [this](const TuyaDatapoint &datapoint) {
      ESP_LOGV(TAG, "MCU reported switch is: %s", ONOFF(datapoint.value_bool));
      this->is_on_ = datapoint.value_bool;
      this->set_state_flag_(water_heater::WATER_HEATER_STATE_ON, this->is_on_);
      if (!this->is_on_) {
        this->set_mode_(water_heater::WATER_HEATER_MODE_OFF);
      } else if (this->mode_ == water_heater::WATER_HEATER_MODE_OFF) {
        // Switched on but no mode reported yet: fall back to ELECTRIC.
        this->set_mode_(water_heater::WATER_HEATER_MODE_ELECTRIC);
      }
      this->publish_state();
    });
  }

  if (this->mode_id_.has_value()) {
    this->parent_->register_listener(*this->mode_id_, [this](const TuyaDatapoint &datapoint) {
      ESP_LOGV(TAG, "MCU reported mode value is: %u", datapoint.value_enum);
      water_heater::WaterHeaterMode mode;
      if (this->is_on_ && this->mode_from_value_(datapoint.value_enum, mode)) {
        this->set_mode_(mode);
        this->publish_state();
      }
    });
  }

  if (this->target_temperature_id_.has_value()) {
    this->parent_->register_listener(*this->target_temperature_id_, [this](const TuyaDatapoint &datapoint) {
      float value = datapoint.value_int * this->target_temperature_multiplier_;
      ESP_LOGV(TAG, "MCU reported target temperature is: %.1f", value);
      this->set_target_temperature_(value);
      this->publish_state();
    });
  }

  if (this->current_temperature_id_.has_value()) {
    this->parent_->register_listener(*this->current_temperature_id_, [this](const TuyaDatapoint &datapoint) {
      float value = datapoint.value_int * this->current_temperature_multiplier_;
      ESP_LOGV(TAG, "MCU reported current temperature is: %.1f", value);
      this->set_current_temperature(value);
      this->publish_state();
    });
  }
}

water_heater::WaterHeaterCallInternal TuyaWaterHeater::make_call() {
  return water_heater::WaterHeaterCallInternal(this);
}

void TuyaWaterHeater::control(const water_heater::WaterHeaterCall &call) {
  auto mode_val = call.get_mode();
  auto on_val = call.get_on();

  // Determine the desired on/off state. An explicit on/off request wins; otherwise a mode of
  // OFF means off and any other mode means on.
  optional<bool> want_on = on_val;
  if (mode_val.has_value() && !want_on.has_value()) {
    want_on = *mode_val != water_heater::WATER_HEATER_MODE_OFF;
  }

  if (want_on.has_value() && this->switch_id_.has_value()) {
    ESP_LOGV(TAG, "Setting switch: %s", ONOFF(*want_on));
    this->parent_->set_boolean_datapoint_value(*this->switch_id_, *want_on);
  }

  if (mode_val.has_value() && *mode_val != water_heater::WATER_HEATER_MODE_OFF && this->mode_id_.has_value()) {
    uint8_t value;
    if (this->value_from_mode_(*mode_val, value)) {
      ESP_LOGV(TAG, "Setting mode value: %u", value);
      this->parent_->set_enum_datapoint_value(*this->mode_id_, value);
    } else {
      ESP_LOGW(TAG, "No mode value configured for requested mode");
    }
  }

  auto target_temp = call.get_target_temperature();
  if (!std::isnan(target_temp) && this->target_temperature_id_.has_value()) {
    ESP_LOGV(TAG, "Setting target temperature: %.1f", target_temp);
    this->parent_->set_integer_datapoint_value(*this->target_temperature_id_,
                                               (int) (target_temp / this->target_temperature_multiplier_));
  }
}

water_heater::WaterHeaterTraits TuyaWaterHeater::traits() {
  water_heater::WaterHeaterTraits traits;

  traits.add_feature_flags(water_heater::WATER_HEATER_SUPPORTS_ON_OFF);
  if (this->current_temperature_id_.has_value()) {
    traits.add_feature_flags(water_heater::WATER_HEATER_SUPPORTS_CURRENT_TEMPERATURE);
  }
  if (this->target_temperature_id_.has_value()) {
    traits.add_feature_flags(water_heater::WATER_HEATER_SUPPORTS_TARGET_TEMPERATURE);
  }
  if (!this->supported_modes_.empty()) {
    traits.set_supported_modes(this->supported_modes_);
    traits.add_feature_flags(water_heater::WATER_HEATER_SUPPORTS_OPERATION_MODE);
  }
  return traits;
}

bool TuyaWaterHeater::mode_from_value_(uint8_t value, water_heater::WaterHeaterMode &mode) const {
  if (this->eco_value_ == value) {
    mode = water_heater::WATER_HEATER_MODE_ECO;
  } else if (this->electric_value_ == value) {
    mode = water_heater::WATER_HEATER_MODE_ELECTRIC;
  } else if (this->performance_value_ == value) {
    mode = water_heater::WATER_HEATER_MODE_PERFORMANCE;
  } else if (this->high_demand_value_ == value) {
    mode = water_heater::WATER_HEATER_MODE_HIGH_DEMAND;
  } else if (this->heat_pump_value_ == value) {
    mode = water_heater::WATER_HEATER_MODE_HEAT_PUMP;
  } else if (this->gas_value_ == value) {
    mode = water_heater::WATER_HEATER_MODE_GAS;
  } else {
    return false;
  }
  return true;
}

bool TuyaWaterHeater::value_from_mode_(water_heater::WaterHeaterMode mode, uint8_t &value) const {
  optional<uint8_t> mapped;
  switch (mode) {
    case water_heater::WATER_HEATER_MODE_ECO:
      mapped = this->eco_value_;
      break;
    case water_heater::WATER_HEATER_MODE_ELECTRIC:
      mapped = this->electric_value_;
      break;
    case water_heater::WATER_HEATER_MODE_PERFORMANCE:
      mapped = this->performance_value_;
      break;
    case water_heater::WATER_HEATER_MODE_HIGH_DEMAND:
      mapped = this->high_demand_value_;
      break;
    case water_heater::WATER_HEATER_MODE_HEAT_PUMP:
      mapped = this->heat_pump_value_;
      break;
    case water_heater::WATER_HEATER_MODE_GAS:
      mapped = this->gas_value_;
      break;
    default:
      break;
  }
  if (mapped.has_value()) {
    value = *mapped;
    return true;
  }
  return false;
}

void TuyaWaterHeater::dump_config() {
  LOG_WATER_HEATER("", "Tuya Water Heater", this);
  if (this->switch_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Switch has datapoint ID %u", *this->switch_id_);
  if (this->mode_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Mode has datapoint ID %u", *this->mode_id_);
  if (this->target_temperature_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Target Temperature has datapoint ID %u", *this->target_temperature_id_);
  if (this->current_temperature_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Current Temperature has datapoint ID %u", *this->current_temperature_id_);
}

}  // namespace esphome::tuya
