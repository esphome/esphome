#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::sm300d2 {

class SM300D2Sensor final : public PollingComponent, public uart::UARTDevice {
  SUB_SENSOR(co2)
  SUB_SENSOR(formaldehyde)
  SUB_SENSOR(tvoc)
  SUB_SENSOR(pm_2_5)
  SUB_SENSOR(pm_10_0)
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)

 public:
  void update() override;
  void dump_config() override;
};

}  // namespace esphome::sm300d2
