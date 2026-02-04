#pragma once

#include "esphome/components/packet_interface/packet_interface.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace uart {

using namespace packet_interface;

/**
 * A packet interface for sending and receiving framed packets over a UART connection.
 * Uses simple byte-stuffing framing with FLAG_BYTE and CONTROL_BYTE.
 * The protocol wraps data between FLAG_BYTEs.
 * Any occurrence of FLAG_BYTE or CONTROL_BYTE in the data is escaped by emitting CONTROL_BYTE followed by the byte
 * XORed with 0x20.
 */

class UartPacketInterface : public PacketInterface, public UARTDevice {
 public:
  void loop() override;
  void set_rx_buffer_size(size_t rx_buffer_size) { this->rx_buffer_size_ = rx_buffer_size; }
  bool send_to_interface(const PacketBuffer &data, PacketMetaData) override;
  size_t get_max_packet_size() override;

 protected:
  void write_byte_(uint8_t byte);
  std::vector<uint8_t> receive_buffer_{};
  size_t rx_buffer_size_{1024};
  bool rx_started_{};
  bool rx_control_{};
};

}  // namespace uart
}  // namespace esphome
