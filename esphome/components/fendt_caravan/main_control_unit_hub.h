#pragma once

#ifdef USE_ESP32
#include <cmath>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "fendt_caravan_hub_base.h"
#include "esphome/core/log.h"
#include "device_decoders.h"
#include "device_commands.h"
#include "fendt_caravan.h"

namespace esphome::fendt_caravan {

class MainControlUnitHub : public FendtCaravanHubBase, public Parented<FendtCaravan> {
 public:
  void dump_config() override;
  void update() override;
  void write_state(switch_::Switch *sw, bool state);

  SUB_SWITCH(main_switch);
  SUB_SENSOR(temp_in);
  SUB_SENSOR(temp_out);
  SUB_BINARY_SENSOR(power_status);
  SUB_TEXT_SENSOR(software_version);
  SUB_SWITCH(all_lights);
  SUB_SWITCH(floor_heater);
  SUB_SENSOR(water_level);
  SUB_TEXT_SENSOR(panel_version);

 protected:
  bool decode(const char *key, const char *value) override;

 private:
  int hs_key_state_ = -1;
  // Latest reported values, published by update()
  float temp_in_{NAN};
  float temp_out_{NAN};
};
}  // namespace esphome::fendt_caravan
#endif
