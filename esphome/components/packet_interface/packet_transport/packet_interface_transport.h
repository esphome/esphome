#pragma once

#include "esphome/components/packet_interface/packet_interface.h"
#include "esphome/components/packet_transport/packet_transport.h"

namespace esphome::packet_interface {

using namespace packet_transport;
using namespace packet_interface;
/**
 * A transport protocol adapter that bridges PacketInterface to PacketTransport.
 * This allows packet_transport functionality to be used with any PacketInterface implementation.
 */
class PacketInterfaceTransport : public PacketTransport {
 public:
  PacketInterfaceTransport(PacketInterface *interface) : interface_(interface) {}

  void setup() override {
    PacketTransport::setup();
    this->interface_->add_packet_interface_listener([this](const PacketBuffer &data, PacketMetaData meta_data) {
      // PacketTransport expects std::vector, so convert
      std::vector<uint8_t> vec = data.to_vector();
      this->process_(vec);
    });
  }

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

 protected:
  // implementing PacketTransport virtual methods
  size_t get_max_packet_size() override { return this->interface_->get_max_packet_size(); }
  void send_packet(const std::vector<uint8_t> &buf) const override {
    PacketBuffer buffer(buf);
    this->interface_->send_to_interface(buffer, {});
  }
  PacketInterface *interface_;
};

}  // namespace esphome::packet_interface
