#pragma once

#include "esphome/components/packet_interface/packet_interface.h"
#include "esphome/components/packet_transport/packet_transport.h"

namespace esphome::packet_interface {

/**
 * A transport protocol adapter that bridges PacketInterface to PacketTransport.
 * This allows packet_transport functionality to be used with any PacketInterface implementation.
 */
class PacketInterfaceTransport : public packet_transport::PacketTransport {
 public:
  explicit PacketInterfaceTransport(PacketInterface *interface) : interface_(interface) {}

  void setup() override;

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

 protected:
  // implementing PacketTransport virtual methods
  size_t get_max_packet_size() override { return this->interface_->top()->get_max_packet_size(); }
  void send_packet(const std::vector<uint8_t> &buf) const override;
  void receive_(const PacketBuffer &data);

  PacketInterface *interface_;
};

}  // namespace esphome::packet_interface
