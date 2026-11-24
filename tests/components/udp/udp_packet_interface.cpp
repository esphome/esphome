#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "esphome/components/logger/logger.h"
#include "esphome/components/udp/packet_interface/udp_packet_interface.h"
#include "esphome/core/application.h"

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

static constexpr std::array<uint8_t, 4> PEER{192, 168, 1, 60};

class TestUdp : public UdpBufferStorage, public UdpPacketInterface {
 public:
  explicit TestUdp(UDPComponent *udp, std::array<uint8_t, 4> peer = {})
      : UdpPacketInterface(udp, this->tx_buffer_storage, peer) {
    this->add_packet_interface_listener(
        [this](const PacketBuffer &data, const PacketMetaData &) { this->received.push_back(to_vector(data)); });
  }
  using UdpPacketInterface::accepts_from_;
  using UdpPacketInterface::receive_;

  std::vector<Packet> received;
};

static sockaddr_in ipv4(std::array<uint8_t, 4> address) {
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  memcpy(&addr.sin_addr.s_addr, address.data(), address.size());
  return addr;
}

// The udp component is not set up, so it has no destinations and a send does nothing
class UdpPacketInterfaceTest : public ::testing::Test {
 protected:
  UDPComponent udp_;
  TestUdp link_{&this->udp_};
  sockaddr_in sender_{ipv4({10, 0, 0, 1})};
};

TEST_F(UdpPacketInterfaceTest, ReceivedPacketsUpToMaxPassedUp) {
  Packet packet{1, 2, 3, 4, 5, 6, 7, 8};
  this->link_.receive_(packet, reinterpret_cast<sockaddr *>(&this->sender_));
  Packet oversized{1, 2, 3, 4, 5, 6, 7, 8, 9};
  this->link_.receive_(oversized, reinterpret_cast<sockaddr *>(&this->sender_));
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

TEST_F(UdpPacketInterfaceTest, WithoutPeerAcceptsAnySender) {
  EXPECT_FALSE(this->link_.has_capability(packet_interface::CAPABILITY_ADDRESSING));
  EXPECT_TRUE(this->link_.accepts_from_(reinterpret_cast<sockaddr *>(&this->sender_)));
}

TEST_F(UdpPacketInterfaceTest, WithPeerAcceptsOnlyPeer) {
  TestUdp link(&this->udp_, PEER);
  EXPECT_TRUE(link.has_capability(packet_interface::CAPABILITY_ADDRESSING));
  auto peer = ipv4(PEER);
  Packet packet{1, 2};
  link.receive_(packet, reinterpret_cast<sockaddr *>(&this->sender_));
  EXPECT_TRUE(link.received.empty());
  link.receive_(packet, reinterpret_cast<sockaddr *>(&peer));
  EXPECT_EQ(link.received, std::vector<Packet>{packet});
}

// Collects each warning logged
class Warnings {
 public:
  Warnings() : previous_(logger::global_logger) {
    this->logger_.pre_setup();
    this->logger_.set_log_level(ESPHOME_LOG_LEVEL_WARN);
    this->logger_.add_log_callback(this, [](void *instance, uint8_t, const char *, const char *message, size_t len) {
      static_cast<Warnings *>(instance)->lines.emplace_back(message, len);
    });
  }
  ~Warnings() { logger::global_logger = this->previous_; }

  std::vector<std::string> lines;

 private:
  logger::Logger *previous_;
  logger::Logger logger_{0};
};

TEST_F(UdpPacketInterfaceTest, DroppedSenderWarnedOnceAMinute) {
  TestUdp link(&this->udp_, PEER);
  Warnings warnings;
  Packet packet{1, 2};
  auto drop_at = [&](uint32_t now) {
    LoopBlockingGuard guard{nullptr, nullptr, now};
    link.receive_(packet, reinterpret_cast<sockaddr *>(&this->sender_));
  };
  drop_at(1000);
  ASSERT_EQ(warnings.lines.size(), 1u);
  EXPECT_NE(warnings.lines[0].find("dropped"), std::string::npos);
  drop_at(30000);
  drop_at(60999);
  EXPECT_EQ(warnings.lines.size(), 1u);
  drop_at(61000);
  EXPECT_EQ(warnings.lines.size(), 2u);
  EXPECT_TRUE(link.received.empty());
}

TEST_F(UdpPacketInterfaceTest, WithPeerDropsSenderNotIpv4) {
  TestUdp link(&this->udp_, PEER);
  sockaddr other{};
  other.sa_family = AF_UNSPEC;
  EXPECT_FALSE(link.accepts_from_(&other));
}

}  // namespace esphome::udp::testing

#endif
