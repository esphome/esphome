#pragma once

#include "esphome/core/defines.h"

// udp_component.h declares nothing without USE_NETWORK
#ifdef USE_NETWORK
#include <array>
#include <span>

#include "esphome/components/packet_interface/packet_interface.h"
#include "esphome/components/socket/socket.h"
#include "../udp_component.h"

namespace esphome::udp {

/**
 * A packet interface link over UDP: each packet is one datagram, sent to every address of the UDP component.
 *
 * When the UDP component sends to one unicast IPv4 address, the link joins exactly two devices: it accepts packets
 * only from that address, and provides the addressing capability itself. Otherwise every packet is accepted, and an
 * addressing layer can tell the devices apart.
 */
class UdpPacketInterface : public packet_interface::PacketInterface {
 public:
  /// @param tx_buffer Joins packets split across buffers; its size is the largest packet, and it must outlive the link
  /// @param peer The IPv4 address of the one peer, in network byte order; all zeros if packets are broadcast
  UdpPacketInterface(UDPComponent *udp_component, std::span<uint8_t> tx_buffer, std::array<uint8_t, 4> peer);

  void setup() override;
  bool send_to_interface(const packet_interface::PacketBuffer &data,
                         const packet_interface::PacketMetaData &meta_data) override;

 protected:
  void receive_(std::span<const uint8_t> data, const sockaddr *source);
  /// True if packets from \p source are accepted: from anywhere, or with a peer, only from the peer.
  bool accepts_from_(const sockaddr *source) const;

  UDPComponent *parent_;
  const std::span<uint8_t> tx_buffer_;
  std::array<uint8_t, 4> peer_;
};
}  // namespace esphome::udp
#endif
