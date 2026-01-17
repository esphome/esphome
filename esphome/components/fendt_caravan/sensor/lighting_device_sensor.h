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
  void setup() override;
  void dump_config() override;

 protected:
  FENDT_SWITCH(light_sw0);
  FENDT_SWITCH(light_sw1);
  FENDT_SWITCH(light_sw2);
  FENDT_SWITCH(light_sw3);
  FENDT_LIGHT_OUTPUT(light_dimsw0);
  FENDT_LIGHT_OUTPUT(light_dimsw1);
  FENDT_LIGHT_OUTPUT(light_dimsw2);
  FENDT_LIGHT_OUTPUT(light_dimsw3);
  FENDT_LIGHT_OUTPUT(light_dimsw4);
  const char *get_tag() override { return this->TAG; }

 private:
  const char *TAG = "LDS";
  void on_switch_state_changed(FendtSwitch *sw, bool state);
  void on_light_output_state_changed(FendtLightOutput *lo, lamp_state_t state);
  Variable<lamp_state_t> *create_variable(const std::string &name, FendtLightOutput *lo) {
    auto *dimsw = lo->create_variable(
        name,
        [](const std::string &data) {
          int value = Coders::decode_int(data);
          lamp_state_t ls = {.status = value != 0, .state = (float) value / 15.0f};
          return ls;
        },
        Commands::update_toggle<lamp_state_t>,
        [](const std::string &name, lamp_state_t state) { return Commands::update_int(name, state.state * 15.f); });
    return dimsw;
  }
};
}  // namespace fendt_caravan
}  // namespace esphome
