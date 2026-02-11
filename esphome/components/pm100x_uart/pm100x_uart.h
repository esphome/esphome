#pragma once

#include "esphome/core/helpers.h"
#include "esphome/components/pm100x/pm100x.h"
#include "esphome/components/uart/uart.h"

namespace esphome::pm100x_uart {

// UART-enabled subclass with protocol parsing
class PM100XComponentUART : public pm100x::PM100XComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

 protected:
  optional<bool> check_byte_() const;
  void parse_data_();
  const uint8_t *get_response_header_(size_t &length) const;
  const uint8_t *get_command_measure_(size_t &length) const;
  size_t get_frame_data_length_() const;
  uint8_t pm100x_checksum_(const uint8_t *command_data, size_t length) const;
  uint16_t get_16_bit_uint_(uint8_t start_index) const {
    return encode_uint16(this->data_[start_index], this->data_[start_index + 1]);
  }

  uint8_t data_[20];
  uint8_t data_index_{0};
};

}  // namespace esphome::pm100x_uart
