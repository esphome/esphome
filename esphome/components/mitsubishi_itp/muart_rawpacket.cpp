#include "muart_rawpacket.h"

namespace esphome {
namespace mitsubishi_itp {

// Creates an empty packet
RawPacket::RawPacket(PacketType packet_type, uint8_t payload_size, SourceBridge source_bridge,
                     ControllerAssociation controller_association)
    : length_{(uint8_t) (payload_size + PACKET_HEADER_SIZE + 1)},
      checksum_index_{(uint8_t) (length_ - 1)},
      source_bridge_{source_bridge},
      controller_association_{controller_association} {
  memcpy(packet_bytes_, EMPTY_PACKET, length_);
  packet_bytes_[PACKET_HEADER_INDEX_PACKET_TYPE] = static_cast<uint8_t>(packet_type);
  packet_bytes_[PACKET_HEADER_INDEX_PAYLOAD_LENGTH] = payload_size;

  update_checksum_();
}

// Creates a packet with the provided bytes
RawPacket::RawPacket(const uint8_t packet_bytes[], const uint8_t packet_length, SourceBridge source_bridge,
                     ControllerAssociation controller_association)
    : length_{(uint8_t) packet_length},
      checksum_index_{(uint8_t) (packet_length - 1)},
      source_bridge_{source_bridge},
      controller_association_{controller_association} {
  memcpy(packet_bytes_, packet_bytes, packet_length);

  if (!this->is_checksum_valid()) {
    // For now, just log this as information (we can decide if we want to process it elsewhere)
    ESP_LOGI(PTAG, "Packet of type %x has invalid checksum!", this->get_packet_type());
  }
}

// Creates an empty RawPacket
RawPacket::RawPacket() {
  // TODO: Is this okay?
}

uint8_t RawPacket::calculate_checksum_() const {  // NOLINT(readability-identifier-naming)
  uint8_t sum = 0;
  for (int i = 0; i < checksum_index_; i++) {
    sum += packet_bytes_[i];
  }

  return (0xfc - sum) & 0xff;
}

RawPacket &RawPacket::update_checksum_() {
  packet_bytes_[checksum_index_] = calculate_checksum_();
  return *this;
}

bool RawPacket::is_checksum_valid() const { return packet_bytes_[checksum_index_] == calculate_checksum_(); }

// Sets a payload byte and automatically updates the packet checksum
RawPacket &RawPacket::set_payload_byte(const uint8_t payload_byte_index, const uint8_t value) {
  packet_bytes_[PACKET_HEADER_SIZE + payload_byte_index] = value;
  update_checksum_();
  return *this;
}

RawPacket &RawPacket::set_payload_bytes(const uint8_t begin_index, const void *value, const size_t size) {
  memcpy(&packet_bytes_[PACKET_HEADER_SIZE + begin_index], value, size);
  update_checksum_();
  return *this;
}

}  // namespace mitsubishi_itp
}  // namespace esphome
