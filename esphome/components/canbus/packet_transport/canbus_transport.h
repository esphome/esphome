#pragma once

#include "esphome/core/component.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include "../canbus.h"
#include <vector>

namespace esphome {
namespace canbus {

/**
 * A transport protocol for sending and receiving packets over a CAN bus.
 *
 * Since CAN frames are limited to 8 bytes, packets are fragmented across multiple frames.
 * Frame format:
 *   Byte 0: Sequence number and flags (bit 7 = last frame, bits 0-6 = sequence 0-127)
 *   Bytes 1-7: Payload data (7 bytes per frame)
 *
 * A dedicated CAN ID is used for packet transport frames.
 */
static const uint16_t MAX_PACKET_SIZE = 508;  // Match UART transport limit
static const uint8_t PAYLOAD_BYTES_PER_FRAME = 7;  // 1 byte for header, 7 for data
static const uint8_t LAST_FRAME_FLAG = 0x80;
static const uint8_t SEQUENCE_MASK = 0x7F;

class CanbusTransport : public packet_transport::PacketTransport, public Parented<Canbus> {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  void set_can_id(uint32_t can_id) { this->can_id_ = can_id; }
  void set_use_extended_id(bool use_extended_id) { this->use_extended_id_ = use_extended_id; }

 protected:
  void send_packet(const std::vector<uint8_t> &buf) const override;
  bool should_send() override { return true; }
  size_t get_max_packet_size() override { return MAX_PACKET_SIZE; }

  void handle_can_frame_(uint32_t can_id, bool extended_id, bool rtr, const std::vector<uint8_t> &data);

  uint32_t can_id_{0x600};  // Default CAN ID for packet transport
  bool use_extended_id_{false};
  std::vector<uint8_t> receive_buffer_{};
  uint8_t expected_sequence_{0};
  bool receiving_{false};
};

}  // namespace canbus
}  // namespace esphome
