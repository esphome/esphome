#pragma once

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include <driver/uart.h>
#include <HardwareSerial.h>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "uart_component.h"

namespace esphome {
namespace uart {

class ESP32ArduinoUARTComponent : public UARTComponent, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void write_array(const uint8_t *data, size_t len) override;

  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;

  int available() override;
  void flush() override;

  uint32_t get_config();

  HardwareSerial *get_hw_serial() { return this->hw_serial_; }
  uint8_t get_hw_serial_number() { return this->number_; }

 protected:
  void check_logger_conflict() override;

  HardwareSerial *hw_serial_{nullptr};
  uint8_t number_{0};
};

}  // namespace uart
}  // namespace esphome

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
