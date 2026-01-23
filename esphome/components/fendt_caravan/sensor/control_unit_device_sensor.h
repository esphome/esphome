#pragma once

#ifdef USE_ESP32
#include <functional>
#include "esphome/core/component.h"
#include "esphome/core/string_ref.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "fendt_text_sensor.h"
#include "device_decoders.h"
#include "device_commands.h"
#include "caravan_device.h"
#include "fendt_sensor.h"
#include "fendt_switch.h"
#include "variable.h"

namespace esphome::fendt_caravan {
using namespace std;

class ControlUnitDeviceSensor : public CaravanDevice {
 public:
  void setup() override;
  void dump_config() override;
  void decode(const std::string &name, const std::string &value) override;
  FENDT_SWITCH(main_switch);
  FENDT_SENSOR(temperature_in);
  FENDT_SENSOR(temperature_out);
  FENDT_TEXT_SENSOR(power_status);
  FENDT_SWITCH(light_status);
  FENDT_TEXT_SENSOR(software_version);
  FENDT_SWITCH(floor_heater);

 protected:
 private:
  void on_switch_state_change_(FendtSwitch *sw, bool state);
};

}  // namespace esphome::fendt_caravan

#endif
