#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <vector>
#include "esphome/components/packet_interface/packet_transport/packet_interface_transport.h"
#include "esphome/core/application.h"

namespace esphome::packet_interface::transport_testing {

using Packet = std::vector<uint8_t>;

// A link layer that records what is sent, and passes injected packets to its listeners
class FakeInterface : public PacketInterface {
 public:
  bool send_to_interface(const PacketBuffer &data, const PacketMetaData &) override {
    Packet packet(data.size());
    data.copy_to(packet.data(), packet.size());
    this->sent.push_back(packet);
    return true;
  }
  void deliver(const PacketBuffer &data) { this->on_receive_from_interface_(data, {}); }

  std::vector<Packet> sent;
};

// A layer stacked on a link, which the bridge must use in place of the link
class FakeLayer : public FakeInterface {
 public:
  explicit FakeLayer(PacketInterface *lower) { this->stack_on_(lower, CAPABILITY_ENCRYPTION); }
};

class TestBridge : public PacketInterfaceTransport {
 public:
  using PacketInterfaceTransport::PacketInterfaceTransport;
  using PacketInterfaceTransport::send_packet;
  using packet_transport::PacketTransport::ping_keys_;
};

// A packet_transport ping packet; with an encryption key set, process_() stores the sender's key
static Packet ping_packet(const char *hostname, uint32_t key) {
  Packet packet{0x48, 0x50, static_cast<uint8_t>(strlen(hostname))};
  packet.insert(packet.end(), hostname, hostname + strlen(hostname));
  for (int shift = 0; shift != 32; shift += 8)
    packet.push_back(static_cast<uint8_t>(key >> shift));
  return packet;
}

// The test main does not construct App, which the transport's setup() reads the device name from
class PacketInterfaceTransportTest : public ::testing::Test {
 protected:
  void SetUp() override {
    new (&App) Application;
    static char name[] = "bridge-tests";
    static char friendly_name[] = "";
    App.pre_setup(name, sizeof(name) - 1, friendly_name, 0);
    this->bridge_.set_encryption_key(std::vector<uint8_t>(32, 0xAA));
    this->bridge_.setup();
  }
  void TearDown() override { App.~Application(); }

  FakeInterface link_;
  FakeLayer layer_{&this->link_};
  TestBridge bridge_{&this->link_};
};

TEST_F(PacketInterfaceTransportTest, SendsThroughTopOfStack) {
  this->bridge_.send_packet({1, 2, 3});
  EXPECT_EQ(this->layer_.sent, (std::vector<Packet>{{1, 2, 3}}));
  EXPECT_TRUE(this->link_.sent.empty());
}

TEST_F(PacketInterfaceTransportTest, PacketInOneBufferReachesTransport) {
  Packet ping = ping_packet("requester", 0xDEADBEEF);
  this->layer_.deliver(PacketBuffer(ping));
  ASSERT_EQ(this->bridge_.ping_keys_.size(), 1u);
  EXPECT_EQ(this->bridge_.ping_keys_["requester"], 0xDEADBEEFu);
}

TEST_F(PacketInterfaceTransportTest, PacketSplitAcrossBuffersDropped) {
  Packet ping = ping_packet("requester", 0xDEADBEEF);
  this->layer_.deliver(PacketBuffer(ping.data(), 4, ping.data() + 4, ping.size() - 4));
  EXPECT_TRUE(this->bridge_.ping_keys_.empty());
}

}  // namespace esphome::packet_interface::transport_testing
