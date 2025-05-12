#pragma once

#include "../udp_component.h"
#ifdef USE_NETWORK
#include "esphome/core/component.h"
#include "esphome/components/packet_transport/packet_transport.h"
#include <vector>

namespace esphome {
namespace udp {

class UDPTransport : public packet_transport::PacketTransport, public Parented<UDPComponent> {
 public:
  void setup() override;
  void update() override;

  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  void send_packet(const std::vector<uint8_t> &buf) const override;
  bool should_send() override;
  bool should_broadcast_{false};
  size_t get_max_packet_size() override { return MAX_PACKET_SIZE; }
};

}  // namespace udp
}  // namespace esphome
#endif
