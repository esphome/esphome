#pragma once

#include "esphome/components/pm100x/pm100x.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace pm100x_uart {

// UART-enabled subclass
class PM100XComponentUART : public pm100x::PM100XComponent, public uart::UARTDevice {
 public:
  void loop() override { pm100x::PM100XComponent::loop(); }
  void update() override { pm100x::PM100XComponent::update(); }

 protected:
  bool has_uart() const override { return this->parent_ != nullptr; }
  void write_uart_array(const uint8_t *data, size_t len) override { this->write_array(data, len); }
  bool read_uart_byte(uint8_t *byte) override { return this->read_byte(byte); }
  int uart_available() override { return this->available(); }
  void check_uart_baud_rate(uint32_t baud_rate) override { this->check_uart_settings(baud_rate); }
};

}  // namespace pm100x_uart
}  // namespace esphome
