#pragma once

#include "esphome/core/component.h"
#include "esphome/components/transport/transport.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace uart {

class UartTransport : public transport::Transport, public UARTDevice {
 public:
  void loop() override;
  void set_rx_buffer_size(size_t rx_buffer_size) { this->rx_buffer_size_ = rx_buffer_size; }

 protected:
  bool send_data_(const std::vector<uint8_t> &data) override;

  std::vector<uint8_t> rx_data_{};
  size_t rx_buffer_size_{1024};
};

}  // namespace uart
}  // namespace esphome
