#pragma once

#ifdef USE_ESP32
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/string_ref.h"
#include "fendt_caravan_hub_base.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "device_commands.h"
#include "device_decoders.h"
#include "fendt_caravan.h"

namespace esphome::fendt_caravan {

using namespace std;

#define SUB_CLIMATE(name) \
 protected: \
  climate::Climate *name##_climate_{nullptr}; \
\
 public: \
  void set_##name##_climate(climate::Climate *c) { this->name##_climate_ = c; }

class AldeDeviceHub : public FendtCaravanHubBase, public sensor::Sensor, public Parented<FendtCaravan> {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  SUB_BINARY_SENSOR(alde_status);
  SUB_CLIMATE(alde);
  SUB_SWITCH(alde_heater);
  SUB_SWITCH(alde_water_heater);
  SUB_SWITCH(alde_water_heater_boost);
  SUB_SELECT(alde_heater_electricity);
  SUB_SWITCH(alde_heater_gas);

 protected:
  bool decode(const char *key, const char *value) override;
};
}  // namespace esphome::fendt_caravan
#endif
