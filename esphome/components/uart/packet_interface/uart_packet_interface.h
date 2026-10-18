#pragma once

#include <memory>

#include "esphome/components/packet_interface/packet_interface.h"
#include "esphome/components/uart/uart.h"

namespace esphome::uart {

/**
 * A packet interface for sending and receiving framed packets over a UART connection.
 * Uses simple byte-stuffing framing with FLAG_BYTE and CONTROL_BYTE.
 * The protocol wraps data between FLAG_BYTEs.
 * Any occurrence of FLAG_BYTE or CONTROL_BYTE in the data is escaped by emitting CONTROL_BYTE followed by the byte
 * XORed with 0x20.
 */
class UartPacketInterface : public packet_interface::PacketInterface, public UARTDevice {
 public:
  void setup() override;
  void loop() override;
  bool send_to_interface(const packet_interface::PacketBuffer &data,
                         const packet_interface::PacketMetaData &meta_data) override;

 protected:
  // Holds max_packet_size_ bytes of unescaped received data
  std::unique_ptr<uint8_t[]> rx_buffer_;
  size_t rx_length_{0};
  bool rx_started_{};
  bool rx_control_{};
};

}  // namespace esphome::uart
