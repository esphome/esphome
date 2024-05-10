#include "muart_rawpacket.h"

namespace esphome {
namespace mitsubishi_uart {

// Creates an empty packet
RawPacket::RawPacket(PacketType packet_type, uint8_t payload_size, SourceBridge source_bridge,
                     ControllerAssociation controller_association)
    : length{(uint8_t) (payload_size + PACKET_HEADER_SIZE + 1)},
      checksumIndex{(uint8_t) (length - 1)},
      sourceBridge{source_bridge},
      controllerAssociation{controller_association} {
  memcpy(packetBytes, EMPTY_PACKET, length);
  packetBytes[PACKET_HEADER_INDEX_PACKET_TYPE] = static_cast<uint8_t>(packet_type);
  packetBytes[PACKET_HEADER_INDEX_PAYLOAD_LENGTH] = payload_size;

  updateChecksum();
}

// Creates a packet with the provided bytes
RawPacket::RawPacket(const uint8_t packet_bytes[], const uint8_t packet_length, SourceBridge source_bridge,
                     ControllerAssociation controller_association)
    : length{(uint8_t) packet_length},
      checksumIndex{(uint8_t) (packet_length - 1)},
      sourceBridge{source_bridge},
      controllerAssociation{controller_association} {
  memcpy(packetBytes, packet_bytes, packet_length);

  if (!this->isChecksumValid()) {
    // For now, just log this as information (we can decide if we want to process it elsewhere)
    ESP_LOGI(PTAG, "Packet of type %x has invalid checksum!", this->getPacketType());
  }
}

// Creates an empty RawPacket
RawPacket::RawPacket() {
  // TODO: Is this okay?
}

uint8_t RawPacket::calculateChecksum() const {
  uint8_t sum = 0;
  for (int i = 0; i < checksumIndex; i++) {
    sum += packetBytes[i];
  }

  return (0xfc - sum) & 0xff;
}

RawPacket &RawPacket::updateChecksum() {
  packetBytes[checksumIndex] = calculateChecksum();
  return *this;
}

bool RawPacket::isChecksumValid() const { return packetBytes[checksumIndex] == calculateChecksum(); }

// Sets a payload byte and automatically updates the packet checksum
RawPacket &RawPacket::setPayloadByte(const uint8_t payload_byte_index, const uint8_t value) {
  packetBytes[PACKET_HEADER_SIZE + payload_byte_index] = value;
  updateChecksum();
  return *this;
}

}  // namespace mitsubishi_uart
}  // namespace esphome
