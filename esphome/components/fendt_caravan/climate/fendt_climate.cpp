#ifdef USE_ESP32
#include "fendt_climate.h"

ESPHOME_LOG_TAG(TAG, "fc.climate");

namespace esphome::fendt_caravan {

Trigger<> *FendtClimate::get_heat_action_trigger() { return &this->heat_action_trigger_; }
Trigger<> *FendtClimate::get_off_action_trigger() { return &this->off_action_trigger_; }

void FendtClimate::set_heater_switch(switch_::Switch *heater_switch) {
  this->heater_switch_ = heater_switch;
  this->heater_switch_->add_on_state_callback([this](bool state) {
    if (state) {
      if (this->mode != climate::CLIMATE_MODE_HEAT) {
        this->mode = climate::CLIMATE_MODE_HEAT;
        this->action = climate::CLIMATE_ACTION_HEATING;
        this->publish_state();
      }
    } else {
      if (this->mode != climate::CLIMATE_MODE_OFF) {
        this->mode = climate::CLIMATE_MODE_OFF;
        this->action = climate::CLIMATE_ACTION_OFF;
        this->publish_state();
      }
    }
  });
}

void FendtClimate::set_temperature_sensor(sensor::Sensor *sensor) {
  this->temperature_sensor_ = sensor;
  this->temperature_sensor_->add_on_state_callback([this](float state) {
    this->current_temperature = state;
    // current temperature changed, publish state
    this->publish_state();
  });
}

void FendtClimate::control(const climate::ClimateCall &call) {
  Trigger<> *trig = &this->heat_action_trigger_;
  bool action_ready = false;
  bool publish_state = false;
  if (call.get_mode().has_value()) {
    climate::ClimateMode mode = call.get_mode().value();
    if (this->mode == mode)
      return;
    this->mode = mode;
    publish_state = true;
    if (mode == climate::CLIMATE_MODE_OFF) {
      this->action = climate::CLIMATE_ACTION_OFF;
      trig = &this->off_action_trigger_;
      action_ready = true;
      if (this->heater_switch_ != nullptr) {
        this->heater_switch_->turn_off();
      }
    } else if (mode == climate::CLIMATE_MODE_HEAT) {
      this->action = climate::CLIMATE_ACTION_HEATING;
      trig = &this->heat_action_trigger_;
      action_ready = true;
      if (this->heater_switch_ != nullptr) {
        this->heater_switch_->turn_on();
      }
    }
    if (action_ready) {
      if (this->prev_action_trigger_ != nullptr) {
        this->prev_action_trigger_->stop_action();
        this->prev_action_trigger_ = nullptr;
      }
      this->prev_action_trigger_ = trig;
      trig->trigger();
    }
  }
  if (publish_state)
    this->publish_state();
  this->control_callback_.call(const_cast<climate::ClimateCall &>(call));
}

climate::ClimateTraits FendtClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE | climate::CLIMATE_SUPPORTS_ACTION);
  traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::ClimateMode::CLIMATE_MODE_HEAT});
  traits.set_visual_min_temperature(5.0);
  traits.set_visual_max_temperature(30.0);
  traits.set_visual_temperature_step(0.5);
  traits.set_supported_presets({
      climate::CLIMATE_PRESET_HOME,
      climate::CLIMATE_PRESET_AWAY,
  });
  return traits;
}

}  // namespace esphome::fendt_caravan
#endif
