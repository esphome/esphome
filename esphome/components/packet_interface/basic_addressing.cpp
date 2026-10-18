#include "basic_addressing.h"
#ifdef USE_PACKET_INTERFACE_BASIC_ADDRESSING
#include "esphome/core/log.h"

namespace esphome::packet_interface {

ESPHOME_LOG_TAG(TAG, "packet_interface.addressing");

void BasicAddressing::setup() {
  this->link_->add_packet_interface_listener(
      [this](const PacketBuffer &data, const PacketMetaData &meta_data) { this->receive_(data, meta_data); });
}

void BasicAddressing::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Basic Addressing on '%s':\n"
                "  Address: 0x%04X",
                this->link_->get_name(), this->address_);
}

bool BasicAddressing::send(const PacketBuffer &data, const PacketMetaData &meta_data, uint16_t destination) {
  if (meta_data.destination_address != 0)
    destination = meta_data.destination_address;
  uint8_t header[HEADER_SIZE] = {static_cast<uint8_t>(destination), static_cast<uint8_t>(destination >> 8),
                                 static_cast<uint8_t>(this->address_), static_cast<uint8_t>(this->address_ >> 8)};
  PacketBuffer packet(header, HEADER_SIZE, &data, nullptr, 0);
  return this->link_->send_to_interface(packet, meta_data);
}

void BasicAddressing::receive_(const PacketBuffer &data, const PacketMetaData &meta_data) {
  // Lower layers deliver packets in one piece; the payload is passed up without copying
  if (!data.is_contiguous()) {
    ESP_LOGW(TAG, "%s: Packet split across buffers is not supported", this->link_->get_name());
    return;
  }
  auto packet = data.as_span();
  if (packet.size() < HEADER_SIZE)
    return;
  uint16_t destination = encode_uint16(packet[1], packet[0]);
  uint16_t source = encode_uint16(packet[3], packet[2]);
  if (source == this->address_ || (destination != this->address_ && destination != BROADCAST_ADDRESS))
    return;
  // A device with a port of its own is heard only there; the broadcast port hears the rest
  AddressingPort *target = nullptr;
  for (auto *port : this->ports_) {
    if (port->destination_ == source) {
      target = port;
      break;
    }
    if (port->destination_ == BROADCAST_ADDRESS)
      target = port;
  }
  if (target == nullptr) {
    ESP_LOGV(TAG, "%s: Packet from 0x%04X dropped", this->link_->get_name(), source);
    return;
  }
  PacketMetaData received = meta_data;
  received.source_address = source;
  received.destination_address = destination;
  target->on_receive_from_interface_(PacketBuffer(packet.data() + HEADER_SIZE, packet.size() - HEADER_SIZE), received);
}

AddressingPort::AddressingPort(BasicAddressing *addressing, uint16_t destination, bool only_port)
    : PacketInterface(
          remaining_packet_size(addressing->get_link()->get_max_packet_size(), BasicAddressing::HEADER_SIZE)),
      addressing_(addressing),
      destination_(destination) {
  if (only_port) {
    this->stack_on_(addressing->get_link(), CAPABILITY_ADDRESSING);
  } else {
    this->inherit_from_(addressing->get_link(), CAPABILITY_ADDRESSING);
  }
  // A link with no room leaves the layers above none either, so they fail their setup
  addressing->add_port(this);
}

void AddressingPort::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Addressing Port '%s':\n"
                "  Max Packet Size: %zu",
                this->name_, this->max_packet_size_);
  if (this->destination_ == BasicAddressing::BROADCAST_ADDRESS) {
    ESP_LOGCONFIG(TAG, "  Destination: broadcast");
  } else {
    ESP_LOGCONFIG(TAG, "  Destination: 0x%04X", this->destination_);
  }
}

}  // namespace esphome::packet_interface
#endif
