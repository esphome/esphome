#pragma once

#include "esphome/core/defines.h"
#ifdef USE_PACKET_INTERFACE_BASIC_ADDRESSING
#include "esphome/core/helpers.h"
#include "packet_interface.h"

namespace esphome::packet_interface {

class AddressingPort;

/**
 * An addressing layer for channels shared by several devices, such as an RS485 bus or a LoRa radio.
 *
 * Each packet starts with a 16-bit destination address and a 16-bit source address, little endian.
 * BROADCAST_ADDRESS sends to every device. A packet is dropped unless it is addressed to this device or broadcast, and
 * a packet from this device's own address, such as an echo on a half-duplex bus, is dropped too.
 *
 * Above this layer are one or more ports, each the bottom of its own stack of layers. A port with a destination
 * address is a point-to-point link to that device: it sends to it and receives only its packets, so the reliability
 * and session layers and the rolling code, which work between two devices only, can be used above it. A port with
 * the broadcast destination sends to every device, and receives packets from devices that have no port of their own.
 */
class BasicAddressing : public Component {
 public:
  static constexpr uint16_t BROADCAST_ADDRESS = 0xFFFF;
  /// Destination and source addresses.
  static constexpr size_t HEADER_SIZE = 4;

  BasicAddressing(PacketInterface *link, uint16_t address) : link_(link), address_(address) {}

  void setup() override;
  void dump_config() override;

  PacketInterface *get_link() const { return this->link_; }
  void add_port(AddressingPort *port) { this->ports_.push_back(port); }
  /// Sends \p data to \p destination, or to the destination in \p meta_data if set.
  bool send(const PacketBuffer &data, const PacketMetaData &meta_data, uint16_t destination);

 protected:
  void receive_(const PacketBuffer &data, const PacketMetaData &meta_data);

  PacketInterface *link_;
  const uint16_t address_;
  StaticVector<AddressingPort *, PACKET_INTERFACE_ADDRESSING_PORT_COUNT> ports_;
};

/// The bottom of the stack of layers for one destination, or for broadcast, on an addressed channel.
class AddressingPort : public PacketInterface {
 public:
  /// With \p only_port, the port is the top of the link, so top() on the link reaches this port's stack.
  AddressingPort(BasicAddressing *addressing, uint16_t destination, bool only_port);

  void dump_config() override;
  bool send_to_interface(const PacketBuffer &data, const PacketMetaData &meta_data) override {
    return this->addressing_->send(data, meta_data, this->destination_);
  }
  uint16_t get_destination() const { return this->destination_; }

 protected:
  friend class BasicAddressing;

  BasicAddressing *addressing_;
  const uint16_t destination_;
};

}  // namespace esphome::packet_interface
#endif
