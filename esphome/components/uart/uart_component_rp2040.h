#pragma once

#ifdef USE_RP2040

#include <SerialPIO.h>
#include <SerialUART.h>

#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "uart_component.h"

namespace esphome {
namespace uart {

class RP2040UartComponent : public UARTComponent, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void write_array(const uint8_t *data, size_t len) override;

  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;

  int available() override;
  void flush() override;

  uint16_t get_config();

  bool is_hw_serial() { return this->hw_serial_; }
  HardwareSerial *get_hw_serial() { return this->serial_; }

 protected:
  void check_logger_conflict() override {}
  bool hw_serial_{false};

  HardwareSerial *serial_{nullptr};
};

}  // namespace uart
}  // namespace esphome

#endif  // USE_RP2040
