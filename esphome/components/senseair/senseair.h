#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace senseair {

class SenseAirComponent : public PollingComponent, public uart::UARTDevice {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_co2_sensor(sensor::Sensor *co2_sensor) { co2_sensor_ = co2_sensor; }

  void update() override;
  void dump_config() override;

 protected:
  uint16_t senseair_checksum_(uint8_t *ptr, uint8_t length);
  bool senseair_write_command_(const uint8_t *command, uint8_t *response);

  sensor::Sensor *co2_sensor_{nullptr};
};

}  // namespace senseair
}  // namespace esphome
