#pragma once

#ifdef USE_ESP32
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/select/select.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "fendt_caravan_hub_base.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "device_commands.h"
#include "device_decoders.h"
#include "fendt_caravan.h"

namespace esphome::fendt_caravan {
class FridgeDeviceHub : public FendtCaravanHubBase, public Parented<FendtCaravan> {
 public:
  void dump_config() override;
  bool decode(const char *key, const char *value) override;

  SUB_BINARY_SENSOR(fridge_status);
  SUB_SWITCH(fridge_status);
  SUB_SELECT(fridge_mode);
  SUB_TEXT_SENSOR(fridge_power_source);
  SUB_TEXT_SENSOR(fridge_type);
  SUB_SELECT(fridge_temperature);
  void write_state(switch_::Switch *sw, bool state) override {};
};

}  // namespace esphome::fendt_caravan
#endif
