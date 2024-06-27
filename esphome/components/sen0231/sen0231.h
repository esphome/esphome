#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

// ref:
// https://wiki.dfrobot.com/Gravity__HCHO_Sensor_SKU__SEN0231

namespace esphome {
namespace sen0231_sensor {

class Sen0231Sensor : public sensor::Sensor, public PollingComponent, public uart::UARTDevice {
 public:
  void update() override;
  void dump_config() override;
  void setup() override;

 protected:
  void read_data_();
};

}  // namespace sen0231_sensor
}  // namespace esphome
