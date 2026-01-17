#pragma once
#ifdef USE_ESP32
#include "esphome/core/component.h"
#include "esphome/core/string_ref.h"
#include "esphome/core/log.h"
#include "variable.h"
#include "caravan_device.h"
#include "fendt_text_sensor.h"
#include "fendt_number.h"
#include "fendt_switch.h"
#include "fendt_select.h"

namespace esphome {
namespace fendt_caravan {
class FridgeDeviceSensor : public CaravanDevice {
 public:
  void setup() override;
  void dump_config() override;

  FENDT_TEXT_SENSOR(fridge_available);
  FENDT_SWITCH(fridge_status);
  FENDT_SELECT(fridge_mode);
  FENDT_TEXT_SENSOR(fridge_source);
  FENDT_TEXT_SENSOR(fridge_type);
  FENDT_NUMBER(fridge_temperature);
  const char *TAG = "FRG";

 protected:
  const char *get_tag_() override { return this->TAG; }

 private:
  void on_switch_state_change_(FendtSwitch *sw, bool state);
  void on_number_state_change_(FendtNumber *num, float state);
  void on_select_state_change_(FendtSelect *sel, std::string state);
};
}  // namespace fendt_caravan
}  // namespace esphome

#endif
