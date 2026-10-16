#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/line_reader.h"
#include "esphome/components/uart/uart.h"

namespace esphome::hrxl_maxsonar_wr {

class HrxlMaxsonarWrComponent final : public sensor::Sensor, public Component, public uart::UARTDevice {
 public:
  // Nothing really public.

  // ========== INTERNAL METHODS ==========
  void loop() override;
  void dump_config() override;

 protected:
  void handle_line_(const char *line, size_t len);

  // "R1234" plus the NUL; the sensor ends each reading with CR and sometimes injects a rogue 0xFF byte
  uart::LineReader<6, '\r', static_cast<char>(0xFF)> line_reader_;
};

}  // namespace esphome::hrxl_maxsonar_wr
