#pragma once

#include "esphome/components/packet_interface/packet_interface.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace uart {

using namespace packet_interface;

class UartPacketInterface : public PacketInterface, public UARTDevice {
 public:
  void loop() override;
  void set_rx_buffer_size(size_t rx_buffer_size) { this->rx_buffer_size_ = rx_buffer_size; }

 protected:
  bool send_data_(const std::vector<uint8_t> &data, PacketMetaData) override;

  std::vector<uint8_t> rx_data_{};
  size_t rx_buffer_size_{1024};
};

}  // namespace uart
}  // namespace esphome
