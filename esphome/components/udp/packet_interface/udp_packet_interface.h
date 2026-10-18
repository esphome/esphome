#pragma once

#include "esphome/core/defines.h"

// udp_component.h declares nothing without USE_NETWORK
#ifdef USE_NETWORK
#include "esphome/components/packet_interface/packet_interface.h"
#include "../udp_component.h"

namespace esphome::udp {

class UdpPacketInterface : public packet_interface::PacketInterface {
 public:
  explicit UdpPacketInterface(UDPComponent *udp_component) : parent_(udp_component) {}

  void setup() override;
  bool send_to_interface(const packet_interface::PacketBuffer &data,
                         const packet_interface::PacketMetaData &meta_data) override;

 protected:
  UDPComponent *parent_;
};
}  // namespace esphome::udp
#endif
