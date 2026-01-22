#pragma once

#include "esphome/components/uart/uart.h"

namespace esphome::serial_channel {

class SerialChannelTraits {
 public:
  void set_baud_rate(uint32_t baud_rate) { this->baud_rate_ = baud_rate; }
  void set_data_bits(uint8_t data_bits) { this->data_bits_ = data_bits; }
  void set_parity(uart::UARTParityOptions parity) { this->parity_ = parity; }
  void set_stop_bits(uint8_t stop_bits) { this->stop_bits_ = stop_bits; }

  uint32_t get_baud_rate() const { return this->baud_rate_; }
  uint8_t get_data_bits() const { return this->data_bits_; }
  uart::UARTParityOptions get_parity() const { return this->parity_; }
  uint8_t get_stop_bits() const { return this->stop_bits_; }

 protected:
  uint32_t baud_rate_{9600};
  uint8_t data_bits_{8};
  uart::UARTParityOptions parity_{uart::UART_CONFIG_PARITY_NONE};
  uint8_t stop_bits_{1};
};

}  // namespace esphome::serial_channel
