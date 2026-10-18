#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/udp/packet_interface/udp_packet_interface.h"

#ifdef USE_NETWORK

namespace esphome::udp::testing {

using packet_interface::PacketBuffer;
using packet_interface::PacketMetaData;
using Packet = std::vector<uint8_t>;

class TestUdp : public UdpPacketInterface {
 public:
  explicit TestUdp(UDPComponent *udp) : UdpPacketInterface(udp) {
    this->set_max_packet_size(8);
    this->add_packet_interface_listener(
        [this](const PacketBuffer &data, const PacketMetaData &) { this->received.push_back(data.to_vector()); });
  }
  using UdpPacketInterface::receive_;

  std::vector<Packet> received;
};

// The udp component is not set up, so it has no destinations and a send does nothing
class UdpPacketInterfaceTest : public ::testing::Test {
 protected:
  UDPComponent udp;
  TestUdp link{&udp};
};

TEST_F(UdpPacketInterfaceTest, ReceivedPacketsUpToMaxPassedUp) {
  Packet packet{1, 2, 3, 4, 5, 6, 7, 8};
  this->link.receive_(packet);
  Packet oversized{1, 2, 3, 4, 5, 6, 7, 8, 9};
  this->link.receive_(oversized);
  EXPECT_EQ(this->link.received, std::vector<Packet>{packet});
}

TEST_F(UdpPacketInterfaceTest, SendsContiguousAndSplitPackets) {
  this->link.setup();
  Packet packet{1, 2, 3, 4};
  EXPECT_TRUE(this->link.send_to_interface(PacketBuffer(packet), {}));
  uint8_t header = 0;
  PacketBuffer body(packet);
  EXPECT_TRUE(this->link.send_to_interface(PacketBuffer(&header, 1, &body, nullptr, 0), {}));
}

TEST_F(UdpPacketInterfaceTest, OversizedPacketNotSent) {
  this->link.setup();
  Packet packet{1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_FALSE(this->link.send_to_interface(PacketBuffer(packet), {}));
  uint8_t header = 0;
  Packet body_data{1, 2, 3, 4, 5, 6, 7, 8};
  PacketBuffer body(body_data);
  EXPECT_FALSE(this->link.send_to_interface(PacketBuffer(&header, 1, &body, nullptr, 0), {}));
}

}  // namespace esphome::udp::testing

#endif
