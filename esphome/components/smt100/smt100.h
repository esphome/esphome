#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/line_reader.h"
#include "esphome/components/uart/uart.h"

namespace esphome::smt100 {

class SMT100Component final : public PollingComponent, public uart::UARTDevice {
  // 33 characters plus the NUL; the longest line, "15000,80.00,100.00,-20.00,12.00", is 31
  static constexpr uint16_t LINE_BUFFER_SIZE = 34;

 public:
  SMT100Component() = default;

  void dump_config() override;
  void loop() override;
  void update() override;

  SUB_SENSOR(counts)
  SUB_SENSOR(permittivity)
  SUB_SENSOR(temperature)
  SUB_SENSOR(moisture)
  SUB_SENSOR(voltage)

 protected:
  LineReader<LINE_BUFFER_SIZE, '\r', '\n'> line_reader_;

  uint32_t last_transmission_{0};
};

}  // namespace esphome::smt100
