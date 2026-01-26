#pragma once

#include "esphome/core/component.h"
#include "esphome/components/transport/transport.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace uart {

class UartTransport : public transport::Transport, public UARTDevice {
 public:
  void loop() override {
    auto cnt = this->available();
    if (cnt > this->rx_buffer_size_) {
      cnt = this->rx_buffer_size_;
    }
    if (cnt > 0) {
      this->rx_data_.resize(cnt);
      this->read_array(this->rx_data_.data(), cnt);
      this->on_receive_data_(this->rx_data_);
      this->rx_data_.clear();
    }
  }
  void set_rx_buffer_size(size_t rx_buffer_size) { this->rx_buffer_size_ = rx_buffer_size; }

 protected:
  bool send_data_(const std::vector<uint8_t> &data) override {
    this->write_array(data.data(), data.size());
    return true;
  }

  std::vector<uint8_t> rx_data_{};
  size_t rx_buffer_size_{1024};
};

}  // namespace uart
}  // namespace esphome
