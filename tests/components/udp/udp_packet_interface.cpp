#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/udp/packet_interface/udp_packet_interface.h"

#ifdef USE_NETWORK

namespace esphome::udp::testing {

// Copies a packet into a vector, for comparing in tests
static std::vector<uint8_t> to_vector(const esphome::packet_interface::PacketBuffer &data) {
  std::vector<uint8_t> result(data.size());
  data.copy_to(result.data(), result.size());
  return result;
}

using packet_interface::PacketBuffer;
using packet_interface::PacketMetaData;
using Packet = std::vector<uint8_t>;

// The copy buffer is a base, so it is set up before the link's constructor sees it
struct UdpBufferStorage {
  uint8_t tx_buffer_storage[8]{};
};

class TestUdp : public UdpBufferStorage, public UdpPacketInterface {
 public:
  explicit TestUdp(UDPComponent *udp) : UdpPacketInterface(udp, this->tx_buffer_storage) {
    this->add_packet_interface_listener(
        [this](const PacketBuffer &data, const PacketMetaData &) { this->received.push_back(to_vector(data)); });
  }
  using UdpPacketInterface::receive_;

  std::vector<Packet> received;
};

// The udp component is not set up, so it has no destinations and a send does nothing
class UdpPacketInterfaceTest : public ::testing::Test {
 protected:
  UDPComponent udp_;
  TestUdp link_{&this->udp_};
};

TEST_F(UdpPacketInterfaceTest, ReceivedPacketsUpToMaxPassedUp) {
  Packet packet{1, 2, 3, 4, 5, 6, 7, 8};
  this->link_.receive_(packet);
  Packet oversized{1, 2, 3, 4, 5, 6, 7, 8, 9};
  this->link_.receive_(oversized);
  EXPECT_EQ(this->link_.received, std::vector<Packet>{packet});
}

TEST_F(UdpPacketInterfaceTest, SendsContiguousAndSplitPackets) {
  this->link_.setup();
  Packet packet{1, 2, 3, 4};
  EXPECT_TRUE(this->link_.send_to_interface(PacketBuffer(packet), {}));
  uint8_t header = 0;
  PacketBuffer body(packet);
  EXPECT_TRUE(this->link_.send_to_interface(PacketBuffer(&header, 1, &body, nullptr, 0), {}));
  // A packet split across buffers is joined in the copy buffer before it is sent
  const auto *storage = this->link_.tx_buffer_storage;
  EXPECT_EQ(Packet(storage, storage + 5), (Packet{0, 1, 2, 3, 4}));
}

TEST_F(UdpPacketInterfaceTest, OversizedPacketNotSent) {
  this->link_.setup();
  Packet packet{1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_FALSE(this->link_.send_to_interface(PacketBuffer(packet), {}));
  uint8_t header = 0;
  Packet body_data{1, 2, 3, 4, 5, 6, 7, 8};
  PacketBuffer body(body_data);
  EXPECT_FALSE(this->link_.send_to_interface(PacketBuffer(&header, 1, &body, nullptr, 0), {}));
}

}  // namespace esphome::udp::testing

#endif
