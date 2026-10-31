#pragma once

#ifdef USE_ESP32
#include "esphome/components/fendt_caravan/fendt_caravan_hub_base.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/automation.h"
#include "esphome/core/log.h"

namespace esphome::fendt_caravan {

class FendtClimate : public climate::Climate, public Parented<FendtCaravanHubBase> {
 public:
  FendtClimate() {
    this->mode = climate::ClimateMode::CLIMATE_MODE_HEAT;
    this->action = climate::CLIMATE_ACTION_HEATING;
  }

  Trigger<> *get_heat_action_trigger();
  Trigger<> *get_off_action_trigger();

  void set_heater_switch(switch_::Switch *heater_switch);
  void set_temperature_sensor(sensor::Sensor *temp_sensor);

 protected:
  void control(const climate::ClimateCall &call) override;
  climate::ClimateTraits traits() override;

  Trigger<> heat_action_trigger_;
  Trigger<> off_action_trigger_;
  Trigger<> *prev_action_trigger_{nullptr};

 private:
  sensor::Sensor *temperature_sensor_{nullptr};
  switch_::Switch *heater_switch_{nullptr};
};
}  // namespace esphome::fendt_caravan
#endif
