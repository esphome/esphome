#pragma once

#ifdef USE_LIBRETINY

#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "uart_component.h"

namespace esphome {
namespace uart {

class LibreTinyUARTComponent : public UARTComponent, public Component {
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

  HardwareSerial *get_hw_serial() { return this->serial_; }
  int8_t get_hw_serial_number() { return this->hardware_idx_; }

 protected:
  void check_logger_conflict() override;

  HardwareSerial *serial_{nullptr};
  int8_t hardware_idx_{-1};
};

}  // namespace uart
}  // namespace esphome

#endif  // USE_LIBRETINY
