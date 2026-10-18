#include <gtest/gtest.h>

#include <csignal>
#include <cstdint>
#include <memory>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "esphome/components/tcp/packet_interface/tcp_packet_interface.h"
#include "esphome/core/application.h"
#include "esphome/core/wake.h"

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
    App.~Application();
    new (&App) Application();
    static char name[] = "tcp-tests";
    static char friendly_name[] = "";
    App.pre_setup(name, sizeof(name) - 1, friendly_name, 0);
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
    new (&App) Application();  // later tests and the test listener use App
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

TEST_F(TcpPacketInterfaceTest, FrameAssembledAcrossReads) {
  End c;
  int fds[2];
  ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
  c.link.link().adopt(std::make_unique<socket::Socket>(fds[0]));
  c.link.loop();
  // A keep-alive, then a 3 byte packet in three writes: half the length, the rest of it, then the payload
  const uint8_t keep_alive[2] = {0x00, 0x00};
  ASSERT_EQ(::write(fds[1], keep_alive, 2), 2);
  c.link.loop();
  const uint8_t length_low = 0x03;
  ASSERT_EQ(::write(fds[1], &length_low, 1), 1);
  c.link.loop();
  const uint8_t length_high_and_start[2] = {0x00, 0xAA};
  ASSERT_EQ(::write(fds[1], length_high_and_start, 2), 2);
  c.link.loop();
  EXPECT_TRUE(c.received.empty());
  const uint8_t rest[2] = {0xBB, 0xCC};
  ASSERT_EQ(::write(fds[1], rest, 2), 2);
  c.link.loop();
  EXPECT_EQ(c.received, (std::vector<Packet>{{0xAA, 0xBB, 0xCC}}));
  EXPECT_TRUE(c.link.is_connected());
  ::close(fds[1]);
}

// Waits in the main loop's select(), which marks the sockets that are ready to read
static void wait_for_sockets() { esphome::internal::wakeable_delay(5); }

// The server role, accepting connections on a loopback port
class TcpServerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    signal(SIGPIPE, SIG_IGN);
    App.~Application();
    new (&App) Application();
    static char name[] = "tcp-tests";
    static char friendly_name[] = "";
    App.pre_setup(name, sizeof(name) - 1, friendly_name, 0);
    set_time(1000);
    this->port = free_port();
    this->server.link.set_server(true);
    this->server.link.set_port(this->port);
  }
  void TearDown() override {
    this->server.link.on_shutdown();
    for (int fd : this->clients)
      ::close(fd);
    App.~Application();
    new (&App) Application();  // later tests and the test listener use App
  }
  static void set_time(uint32_t now) { LoopBlockingGuard guard{nullptr, nullptr, now}; }
  static uint16_t free_port() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t len = sizeof(addr);
    EXPECT_EQ(::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), len), 0);
    EXPECT_EQ(::getsockname(fd, reinterpret_cast<struct sockaddr *>(&addr), &len), 0);
    ::close(fd);
    return ntohs(addr.sin_port);
  }
  // Connects a client and runs the server's loop until it has accepted it
  int connect_client() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(this->port);
    EXPECT_EQ(::connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)), 0);
    this->clients.push_back(fd);
    for (int i = 0; i != 100 && !this->server.link.is_connected(); i++) {
      this->server.link.loop();
      wait_for_sockets();
    }
    return fd;
  }

  End server;
  uint16_t port{0};
  std::vector<int> clients;
};

TEST_F(TcpServerTest, AcceptsClientAndReceives) {
  this->server.link.loop();  // starts listening
  int client = this->connect_client();
  ASSERT_TRUE(this->server.link.is_connected());
  EXPECT_EQ(this->server.states, std::vector<bool>{true});
  const uint8_t frame[3] = {0x01, 0x00, 0x42};
  ASSERT_EQ(::write(client, frame, sizeof(frame)), 3);
  for (int i = 0; i != 100 && this->server.received.empty(); i++) {
    this->server.link.loop();
    wait_for_sockets();
  }
  EXPECT_EQ(this->server.received, std::vector<Packet>{{0x42}});
}

TEST_F(TcpServerTest, AcceptsNextClientAfterDisconnect) {
  this->server.link.loop();
  int first = this->connect_client();
  ASSERT_TRUE(this->server.link.is_connected());
  ::close(first);
  this->clients.clear();
  for (int i = 0; i != 100 && this->server.link.is_connected(); i++) {
    this->server.link.loop();
    wait_for_sockets();
  }
  EXPECT_EQ(this->server.states, (std::vector<bool>{true, false}));
  this->connect_client();
  EXPECT_TRUE(this->server.link.is_connected());
  EXPECT_EQ(this->server.states, (std::vector<bool>{true, false, true}));
}

}  // namespace esphome::tcp::testing

#endif
