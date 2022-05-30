#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace smt100 {

class SMT100Component : public PollingComponent, public uart::UARTDevice {
  static const uint16_t SMT_BOOT_MS = 8000;
  static const uint16_t MAX_LINE_LENGTH = 80;

 public:
  SMT100Component() = default;
  void loop() override;
  void dump_config() override;

  void set_update_interval(uint32_t val) { update_interval_ = val; };

  void set_temperature_sensor(sensor::Sensor *temperature_sensor);
  void set_moisture_sensor(sensor::Sensor *moisture_sensor);

  int readline(int readch, char *buffer, int len);

 protected:
  uint32_t boot_up_time_{0};
  uint32_t last_update_{0};
  uint32_t update_interval_{0};

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *moisture_sensor_{nullptr};
};

}  // namespace smt100
}  // namespace esphome
