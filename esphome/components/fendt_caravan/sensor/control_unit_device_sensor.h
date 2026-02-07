#pragma once

#ifdef USE_ESP32
#include "esphome/components/fendt_caravan/caravan_device_component.h"
#include "esphome/components/fendt_caravan/fendt_caravan.h"
#include "esphome/components/sensor/sensor.h"
#include <functional>
#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/string_ref.h"
#include "esphome/core/log.h"
#include "fendt_text_sensor.h"
#include "device_decoders.h"
#include "device_commands.h"
#include "fendt_sensor.h"
#include "esphome/components/fendt_caravan/variable.h"

namespace esphome::fendt_caravan {
using namespace std;

class ControlUnitDeviceSensor : public CaravanDeviceComponent, public sensor::Sensor {
 public:
  void setup() override;
  void dump_config() override;
  bool decode(const std::string &name, const std::string &value) override;
  SUB_SWITCH(main_switch);
  FENDT_SENSOR(temperature_in);
  FENDT_SENSOR(temperature_out);
  FENDT_TEXT_SENSOR(power_status);
  SUB_SWITCH(light_status);
  FENDT_TEXT_SENSOR(software_version);
  SUB_SWITCH(floor_heater);

 protected:
  DeviceType get_device_type_() override { return DeviceType::DEVICE_TYPE_MCU; };

 private:
  void on_switch_state_change_(switch_::Switch *sw, bool state) override;
};

}  // namespace esphome::fendt_caravan

#endif
