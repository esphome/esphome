#pragma once

#include "esphome/core/component.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include <vector>
#include "../uart.h"

namespace esphome {
namespace uart {

/**
 * A transport protocol for sending and receiving packets over a UART connection.
 * The protocol is based on Asynchronous HDLC framing. (https://en.wikipedia.org/wiki/High-Level_Data_Link_Control)
 * There are two special bytes: FLAG_BYTE and CONTROL_BYTE.
 * A 16-bit CRC is appended to the packet, then
 * the protocol wraps the resulting data between FLAG_BYTEs.
 * Any occurrence of FLAG_BYTE or CONTROL_BYTE in the data is escaped by emitting CONTROL_BYTE followed by the byte
 * XORed with 0x20.
 */
static const uint16_t MAX_PACKET_SIZE = 508;
static const uint8_t FLAG_BYTE = 0x7E;
static const uint8_t CONTROL_BYTE = 0x7D;

class UARTTransport : public packet_transport::PacketTransport, public UARTDevice {
 public:
  void loop() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

 protected:
  void write_byte_(uint8_t byte) const;
  void send_packet(const std::vector<uint8_t> &buf) const override;
  bool should_send() override { return true; };
  size_t get_max_packet_size() override { return MAX_PACKET_SIZE; }
  std::vector<uint8_t> receive_buffer_{};
  bool rx_started_{};
  bool rx_control_{};
};

}  // namespace uart
}  // namespace esphome
