#pragma once

#include "esphome/core/defines.h"

// ifdefed for no *good* reason - but needed to stop CI failures
#ifdef USE_NETWORK
#include "esphome/components/packet_interface/packet_interface.h"
#include "../udp_component.h"

namespace esphome::udp {
using namespace packet_interface;

class UdpPacketInterface : public PacketInterface {
 public:
  UdpPacketInterface(UDPComponent *udp_component) : parent_(udp_component) {
    this->max_packet_size_ = 508;
    udp_component->set_should_listen(true);
    udp_component->set_should_broadcast(true);
  }

  void setup() override {
    this->parent_->add_listener(
        [this](std::vector<uint8_t> &buf) { this->on_receive_from_interface_(PacketBuffer(buf), {}); });
  }

  bool send_to_interface(const PacketBuffer &data, PacketMetaData) override {
    this->parent_->send_packet(data);
    return true;
  };

 protected:
  UDPComponent *parent_;
};
}  // namespace esphome::udp
#endif
