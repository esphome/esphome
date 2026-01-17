#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "caravan_device.h"
#include "fendt_switch.h"
#include "fendt_light_output.h"

namespace esphome {
namespace fendt_caravan {

class LightingDeviceSensor : public CaravanDevice {
 public:
  void setup();
  void dump_config();

  FENDT_SWITCH(light_sw0);
  FENDT_SWITCH(light_sw1);
  FENDT_SWITCH(light_sw2);
  FENDT_SWITCH(light_sw3);
  FENDT_LIGHT_OUTPUT(light_dimsw0);
  FENDT_LIGHT_OUTPUT(light_dimsw1);
  FENDT_LIGHT_OUTPUT(light_dimsw2);
  FENDT_LIGHT_OUTPUT(light_dimsw3);
  FENDT_LIGHT_OUTPUT(light_dimsw4);

  const char *TAG = "LDS";

 protected:
  const char *get_tag() { return this->TAG; }

 private:
  void on_switch_state_changed_(FendtSwitch *sw, bool state);
  void on_light_output_state_changed_(FendtLightOutput *lo, LampStateT state);
  Variable<LampStateT> *create_variable_(const std::string &name, FendtLightOutput *lo) {
    auto *dimsw = lo->create_variable(
        name,
        [](const std::string &data) {
          int value = DeviceDecoders::decode_int(data);
          LampStateT ls = {.status = value != 0, .state = (float) value / 15.0f};
          return ls;
        },
        Commands::update_toggle<LampStateT>,
        [](const std::string &name, LampStateT state) { return Commands::update_int(name, state.state * 15.f); });
    return dimsw;
  }
};
}  // namespace fendt_caravan
}  // namespace esphome
