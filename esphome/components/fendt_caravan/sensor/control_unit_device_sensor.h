#pragma once

#ifdef USE_ESP32
#include <functional>
#include "esphome/core/component.h"
#include "esphome/core/string_ref.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "fendt_text_sensor.h"
#include "caravan_device.h"
#include "fendt_sensor.h"
#include "fendt_switch.h"
#include "variable.h"

namespace esphome {
namespace fendt_caravan {

using namespace std;

// using command_callback = std::function<void(const std::string &)>;

class ControlUnitDeviceSensor : public CaravanDevice {
 public:
  void setup() override;
  void dump_config() override;
  void decode(const std::string &variable, const std::string &value) override;

  FENDT_SWITCH(main_switch);
  FENDT_SENSOR(temperature_in);
  FENDT_SENSOR(temperature_out);
  FENDT_TEXT_SENSOR(power_status);
  FENDT_SWITCH(light_status);
  FENDT_TEXT_SENSOR(software_version);
  FENDT_SWITCH(floor_heater);

 protected:
  const char *get_tag() override { return this->TAG; }

 private:
  const char *TAG = "MCU";
  void on_switch_state_change(FendtSwitch *sw, bool state);
};
}  // namespace fendt_caravan
}  // namespace esphome

#endif
