#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace pzem004t {

class PZEM004T : public PollingComponent, public uart::UARTDevice {
 public:
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_current_sensor(sensor::Sensor *current_sensor) { current_sensor_ = current_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) { power_sensor_ = power_sensor; }

  void loop() override;

  void update() override;

  void dump_config() override;

 protected:
  sensor::Sensor *voltage_sensor_;
  sensor::Sensor *current_sensor_;
  sensor::Sensor *power_sensor_;

  enum PZEM004TReadState {
    SET_ADDRESS = 0xB4,
    READ_VOLTAGE = 0xB0,
    READ_CURRENT = 0xB1,
    READ_POWER = 0xB2,
    DONE = 0x00,
  } read_state_{DONE};

  void write_state_(PZEM004TReadState state);

  uint32_t last_read_{0};
};

}  // namespace pzem004t
}  // namespace esphome
