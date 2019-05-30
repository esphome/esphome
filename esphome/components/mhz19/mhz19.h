#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace mhz19 {

class MHZ19Component : public PollingComponent, public uart::UARTDevice {
 public:
  float get_setup_priority() const override;

  void update() override;
  void dump_config() override;

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_co2_sensor(sensor::Sensor *co2_sensor) { co2_sensor_ = co2_sensor; }

 protected:
  bool mhz19_write_command_(const uint8_t *command, uint8_t *response);

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *co2_sensor_{nullptr};
};

}  // namespace mhz19
}  // namespace esphome
