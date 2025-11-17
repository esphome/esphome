#pragma once

#include "esphome/components/water_heater/water_heater.h"
#include "esphome/components/thermostat/thermostat_climate.h"

namespace esphome {
namespace thermostat {

class ThermostatWaterHeater : public water_heater::WaterHeater {
 public:
  ThermostatWaterHeater();

  void set_sensor(sensor::Sensor *sensor);
  void set_supports_eco(bool eco);

  // overrides
  void setup() override;
  void dump_config() override;
  water_heater::WaterHeaterTraits traits() override;
  void control(const water_heater::WaterHeaterCall &call) override;

 protected:
  thermostat::ThermostatClimate engine_;
  bool supports_eco_{false};
};

}  // namespace thermostat
}  // namespace esphome
