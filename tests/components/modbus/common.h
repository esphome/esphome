#pragma once
#include <cstdint>
#include "esphome/components/uart/uart_component.h"

namespace esphome::modbus::testing {

// A UART that discards all writes, for tests that never inspect the wire.
class NullUART : public uart::UARTComponent {
 public:
  NullUART() { this->set_baud_rate(115200); }
  void write_array(const uint8_t *data, size_t len) override {}
  bool peek_byte(uint8_t *data) override { return false; }
  bool read_array(uint8_t *data, size_t len) override { return false; }
  size_t available() override { return 0; }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif
  void check_logger_conflict() override {}
};

}  // namespace esphome::modbus::testing
