#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace t6615 {

class T6615Component : public PollingComponent, public uart::UARTDevice {
 public:
  float get_setup_priority() const override;

  void setup() override;
  void update() override;
  void dump_config() override;

  void set_co2_sensor(sensor::Sensor *co2_sensor) { co2_sensor_ = co2_sensor; }

 protected:
  uint8_t t6615_write_command_(uint8_t len, const uint8_t *command, uint8_t *response);
  uint16_t get_ppm_();

  sensor::Sensor *co2_sensor_{nullptr};
};

}  // namespace t6615
}  // namespace esphome
