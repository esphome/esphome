#pragma once
#include "esphome/core/application.h"
#include "esphome/core/string_ref.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "fendt_text_sensor.h"
#include "fendt_number.h"
#include "fendt_switch.h"
#include "fendt_sensor.h"
#include "fendt_select.h"
#include "variable.h"
#include "caravan_device.h"

#ifdef USE_ESP32

namespace esphome {
namespace fendt_caravan {

using namespace std;

class AldeDeviceSensor : public CaravanDevice {
 public:
  void setup() override;
  void dump_config() override;

  FENDT_TEXT_SENSOR(alde_available);
  FENDT_SWITCH(alde_heater_satus);
  FENDT_NUMBER(alde_heater_temperature);
  FENDT_SWITCH(alde_heater_water);
  FENDT_SWITCH(alde_heater_water_temperature);
  FENDT_SELECT(alde_heater_electric);
  FENDT_SWITCH(alde_heater_gas);

 protected:
  const char *get_tag() override { return this->TAG; }

 private:
  const char *const TAG = "ALD";
  void on_switch_state_change(FendtSwitch *sw, bool state);
  void on_number_state_change(FendtNumber *num, float state);
  void on_select_state_change(FendtSelect *sel, std::string state);
};
}  // namespace fendt_caravan
}  // namespace esphome

#endif
