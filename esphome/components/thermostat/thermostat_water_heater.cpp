#include "thermostat_water_heater.h"
#include "esphome/core/log.h"

namespace esphome {
namespace thermostat {

static const char *const TAG = "thermostat.water_heater";

ThermostatWaterHeater::ThermostatWaterHeater() {
  // Configure the engine to **only support heating**
  engine_.set_supports_heat(true);
  engine_.set_supports_cool(false);
  engine_.set_supports_heat_cool(false);
  engine_.set_supports_auto(false);
}

void ThermostatWaterHeater::set_sensor(sensor::Sensor *sensor) { engine_.set_sensor(sensor); }

void ThermostatWaterHeater::set_supports_eco(bool eco) {
  supports_eco_ = eco;
  // We will expose ECO preset if true
}

void ThermostatWaterHeater::setup() { engine_.setup(); }

water_heater::WaterHeaterTraits ThermostatWaterHeater::traits() {
  water_heater::WaterHeaterTraits traits;
  traits.set_supports_modes(true);
  traits.set_supports_target_temperature(true);
  if (supports_eco_)
    traits.add_supported_mode(water_heater::WATER_HEATER_MODE_ECO);
  traits.add_supported_mode(water_heater::WATER_HEATER_MODE_HEAT);
  traits.add_supported_mode(water_heater::WATER_HEATER_MODE_OFF);
  return traits;
}

void ThermostatWaterHeater::control(const water_heater::WaterHeaterCall &call) {
  if (call.get_mode().has_value()) {
    switch (*call.get_mode()) {
      case water_heater::WATER_HEATER_MODE_OFF:
        engine_.set_mode(climate::CLIMATE_MODE_OFF);
        break;
      case water_heater::WATER_HEATER_MODE_HEAT:
        engine_.set_mode(climate::CLIMATE_MODE_HEAT);
        break;
      case water_heater::WATER_HEATER_MODE_ECO:
        engine_.change_preset_(climate::CLIMATE_PRESET_ECO);
        break;
    }
  }

  if (call.get_target_temperature().has_value()) {
    engine_.set_target_temperature(*call.get_target_temperature());
  }

  engine_.refresh();
  this->publish_state();
}

void ThermostatWaterHeater::dump_config() {
  ESP_LOGCONFIG(TAG, "Thermostat Water Heater");
  engine_.dump_config();
}

}  // namespace thermostat
}  // namespace esphome
