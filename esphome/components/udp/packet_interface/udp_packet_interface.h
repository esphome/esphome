#pragma once

#include "esphome/core/defines.h"

// udp_component.h declares nothing without USE_NETWORK
#ifdef USE_NETWORK
#include <memory>
#include <span>

#include "esphome/components/packet_interface/packet_interface.h"
#include "../udp_component.h"

namespace esphome::udp {

/**
 * A packet interface link over UDP: each packet is one datagram, sent to every address of the UDP component.
 *
 * Every datagram received on the listen port is passed up, from any sender, so on a shared network an addressing
 * or encryption layer is needed to tell the peer's packets from others.
 */
class UdpPacketInterface : public packet_interface::PacketInterface {
 public:
  explicit UdpPacketInterface(UDPComponent *udp_component) : parent_(udp_component) {}

  void setup() override;
  bool send_to_interface(const packet_interface::PacketBuffer &data,
                         const packet_interface::PacketMetaData &meta_data) override;

 protected:
  void receive_(std::span<const uint8_t> data);

  UDPComponent *parent_;
  // For packets split across buffers, allocated in setup()
  std::unique_ptr<uint8_t[]> tx_buffer_;
};
}  // namespace esphome::udp
#endif
