#include <gtest/gtest.h>

#include <csignal>
#include <cstdint>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "esphome/components/tcp/packet_interface/tcp_packet_interface.h"
#include "esphome/core/application.h"

#ifdef USE_HOST

namespace esphome::tcp::testing {

// Copies a packet into a vector, for comparing in tests
static std::vector<uint8_t> to_vector(const esphome::packet_interface::PacketBuffer &data) {
  std::vector<uint8_t> result(data.size());
  data.copy_to(result.data(), result.size());
  return result;
}

using packet_interface::PacketBuffer;
using packet_interface::PacketMetaData;
using Packet = std::vector<uint8_t>;

class TestTcp : public TcpPacketInterface {
 public:
  TestTcp() : TcpPacketInterface(this->rx_buffer_storage) {
    this->set_max_packet_size(64);
    this->set_host("peer");
    this->set_port(1);
  }
  socket::TcpClientLink &link() { return this->link_; }

  // The base class only keeps the pointer, so the storage can be a member set up after it
  uint8_t rx_buffer_storage[64]{};
};

// A layer stacked on the link, to check that the session state reaches it
class UpperLayer : public packet_interface::PacketInterface {
 public:
  explicit UpperLayer(PacketInterface *lower) { this->stack_on_(lower, packet_interface::CAPABILITY_ENCRYPTION); }
  bool send_to_interface(const PacketBuffer &, const PacketMetaData &) override { return true; }
};

struct End {
  End() {
    this->link.add_packet_interface_listener(
        [this](const PacketBuffer &data, const PacketMetaData &) { this->received.push_back(to_vector(data)); });
    this->link.add_on_session_state_callback([this](bool connected) { this->states.push_back(connected); });
    this->link.setup();
  }
  bool send(const Packet &data) { return this->link.send_to_interface(PacketBuffer(data), {}); }

  TestTcp link;
  std::vector<Packet> received;
  std::vector<bool> states;
};

class TcpPacketInterfaceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    signal(SIGPIPE, SIG_IGN);
    new (&App) Application;
    App.pre_setup("tcp-tests", 9, "", 0);
    set_time(1000);
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    this->a.link.link().adopt(std::make_unique<socket::Socket>(fds[0]));
    this->b.link.link().adopt(std::make_unique<socket::Socket>(fds[1]));
    this->loops(1);
  }
  void TearDown() override {
    this->a.link.link().close();
    this->b.link.link().close();
    App.~Application();
  }
  static void set_time(uint32_t now) { LoopBlockingGuard guard{nullptr, nullptr, now}; }
  void loops(int count) {
    for (int i = 0; i != count; i++) {
      this->a.link.loop();
      this->b.link.loop();
    }
  }

  End a, b;
};

TEST_F(TcpPacketInterfaceTest, ProvidesReliabilitySessionAndAddressing) {
  for (auto capability : {packet_interface::CAPABILITY_RELIABILITY, packet_interface::CAPABILITY_SESSION,
                          packet_interface::CAPABILITY_ADDRESSING})
    EXPECT_TRUE(this->a.link.has_capability(capability));
  EXPECT_EQ(this->a.states, std::vector<bool>{true});
  EXPECT_TRUE(this->a.link.is_connected());
}

TEST_F(TcpPacketInterfaceTest, PacketsBothWays) {
  Packet big(64);
  for (size_t i = 0; i != big.size(); i++)
    big[i] = i;
  ASSERT_TRUE(this->a.send({1, 2, 3}));
  ASSERT_TRUE(this->a.send(big));
  ASSERT_TRUE(this->b.send({9}));
  this->loops(3);
  EXPECT_EQ(this->b.received, (std::vector<Packet>{{1, 2, 3}, big}));
  EXPECT_EQ(this->a.received, std::vector<Packet>{{9}});
  EXPECT_FALSE(this->a.send(Packet(65)));
}

TEST_F(TcpPacketInterfaceTest, SessionReachesLayersAbove) {
  UpperLayer upper(&this->a.link);
  std::vector<bool> states;
  upper.add_on_session_state_callback([&states](bool connected) { states.push_back(connected); });
  EXPECT_TRUE(upper.has_capability(packet_interface::CAPABILITY_SESSION));
  this->b.link.link().close();  // the peer goes away
  this->loops(3);
  EXPECT_EQ(states, std::vector<bool>{false});
}

TEST_F(TcpPacketInterfaceTest, KeepAliveKeepsIdleLinkUp) {
  for (uint32_t now = 1000; now <= 61000; now += 5000) {
    set_time(now);
    this->loops(2);
  }
  EXPECT_TRUE(this->a.link.is_connected());
  EXPECT_TRUE(this->b.link.is_connected());
  EXPECT_TRUE(this->a.received.empty());  // keep-alives are not passed up
}

TEST_F(TcpPacketInterfaceTest, SilentPeerClosesLink) {
  set_time(1000 + 30001);
  this->a.link.loop();  // b has sent nothing for three keep-alive intervals
  EXPECT_FALSE(this->a.link.is_connected());
  EXPECT_EQ(this->a.states, (std::vector<bool>{true, false}));
}

TEST_F(TcpPacketInterfaceTest, OversizedLengthClosesLink) {
  End c;
  int fds[2];
  ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
  c.link.link().adopt(std::make_unique<socket::Socket>(fds[0]));
  c.link.loop();
  uint8_t header[2] = {0xFF, 0x00};  // 255 bytes, more than 64
  ASSERT_EQ(::write(fds[1], header, sizeof(header)), 2);
  c.link.loop();
  EXPECT_FALSE(c.link.is_connected());
  EXPECT_EQ(c.states, (std::vector<bool>{true, false}));
  ::close(fds[1]);
}

TEST_F(TcpPacketInterfaceTest, NotSentWhileDown) {
  this->a.link.link().close();
  this->a.link.loop();
  EXPECT_FALSE(this->a.send({1}));
}

}  // namespace esphome::tcp::testing

#endif
