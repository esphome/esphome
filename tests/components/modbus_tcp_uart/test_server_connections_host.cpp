#include <gtest/gtest.h>

#include <csignal>
#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "esphome/components/modbus_tcp_uart/mbap.h"
#include "esphome/components/modbus_tcp_uart/modbus_tcp_uart.h"
#include "esphome/components/tcp_uart/tcp_uart.h"
#include "esphome/core/application.h"
#include "esphome/core/wake/wake_host.h"

#ifdef USE_HOST
#ifdef USE_TCP_UART_MAX_CONNECTIONS

// Host only: ESP-IDF has no poll.h.
#include <poll.h>

namespace esphome::modbus_tcp_uart::testing {
namespace {

class Socket : public tcp_uart::TcpUart {
 public:
  uint8_t open_connections() const { return static_cast<uint8_t>(__builtin_popcount(this->up_mask_)); }
};

// A Modbus TCP server link on a tcp_uart with two connections. Clients A and B are real TCP
// sockets on loopback, connected at 1000 ms. The test plays the server hub: it reads RTU requests
// and writes replies.
class ModbusServerTwoClients : public ::testing::Test {
 protected:
  void SetUp() override {
    signal(SIGPIPE, SIG_IGN);
    this->at(1000);
    this->port_ = free_port();
    this->socket_.set_server(true);
    this->socket_.set_port(this->port_);
    this->socket_.set_max_connections(2);
    this->socket_.setup();
    this->link_.set_server(true);
    this->loops(1);
    this->a_ = this->connect_client();
    this->b_ = this->connect_client();
    ASSERT_EQ(this->socket_.open_connections(), 2);
  }
  void TearDown() override {
    for (int fd : {this->a_, this->b_}) {
      if (fd >= 0) {
        ::close(fd);
      }
    }
    this->socket_.on_shutdown();
  }

  static uint16_t free_port() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    socklen_t len = sizeof(addr);
    ::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), len);
    ::getsockname(fd, reinterpret_cast<struct sockaddr *>(&addr), &len);
    ::close(fd);
    return ntohs(addr.sin_port);
  }
  // Sets the loop clock.
  static void at(uint32_t ms) { LoopBlockingGuard{nullptr, nullptr, ms}; }
  void loops(int count) {
    for (int i = 0; i < count; i++) {
      internal::wakeable_delay(5);
      this->socket_.loop();
      this->link_.loop();
    }
  }
  int connect_client() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(this->port_);
    EXPECT_EQ(::connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)), 0);
    const uint8_t before = this->socket_.open_connections();
    for (int i = 0; i < 50 && this->socket_.open_connections() == before; i++) {
      this->loops(1);
    }
    return fd;
  }
  // Read holding register `reg` of unit 1, as a Modbus TCP client sends it.
  static std::vector<uint8_t> read_request(uint16_t txn, uint8_t reg) {
    const uint8_t pdu[] = {0x03, 0x00, reg, 0x00, 0x01};
    std::vector<uint8_t> frame(MBAP_HEADER_SIZE + sizeof(pdu));
    write_mbap(frame.data(), frame.size(), txn, 1, pdu, sizeof(pdu));
    return frame;
  }
  // The answer with one register value, as the client expects it.
  static std::vector<uint8_t> read_reply(uint16_t txn, uint8_t value) {
    const uint8_t pdu[] = {0x03, 0x02, 0x00, value};
    std::vector<uint8_t> frame(MBAP_HEADER_SIZE + sizeof(pdu));
    write_mbap(frame.data(), frame.size(), txn, 1, pdu, sizeof(pdu));
    return frame;
  }
  static void send(int fd, const std::vector<uint8_t> &bytes) {
    ASSERT_EQ(::write(fd, bytes.data(), bytes.size()), static_cast<ssize_t>(bytes.size()));
  }
  // What the client received within 200 ms; empty for nothing.
  static std::vector<uint8_t> received(int fd) {
    struct pollfd pfd {
      fd, POLLIN, 0
    };
    if (::poll(&pfd, 1, 200) != 1) {
      return {};
    }
    uint8_t buf[64];
    ssize_t n = ::read(fd, buf, sizeof(buf));
    return n > 0 ? std::vector<uint8_t>(buf, buf + n) : std::vector<uint8_t>{};
  }
  // Runs the loop until the hub has a whole RTU request (8 bytes) and returns its register.
  int hub_takes_request() {
    for (int i = 0; i < 50 && this->link_.available() < 8; i++) {
      this->loops(1);
    }
    uint8_t rtu[8];
    if (this->link_.available() != 8 || !this->link_.read_array(rtu, sizeof(rtu))) {
      return -1;
    }
    return rtu[3];
  }
  // The hub's RTU reply with one register value.
  void hub_answers(uint8_t value) {
    uint8_t rtu[7] = {0x01, 0x03, 0x02, 0x00, value};
    uint16_t crc = crc16(rtu, 5);
    rtu[5] = crc & 0xFF;
    rtu[6] = crc >> 8;
    this->link_.write_array(rtu, sizeof(rtu));
  }

  Socket socket_;
  ModbusTcpUart link_{&this->socket_};
  uint16_t port_{0};
  int a_{-1};
  int b_{-1};
};

TEST_F(ModbusServerTwoClients, EachClientGetsItsOwnReplyWithItsOwnTransactionId) {
  // Both clients use transaction id 1 and ask at the same time: A for register 1, B for register 2.
  send(this->a_, read_request(1, 1));
  send(this->b_, read_request(1, 2));

  EXPECT_EQ(this->hub_takes_request(), 1);
  // B's request waits until A has its reply.
  this->loops(3);
  EXPECT_EQ(this->link_.available(), 0u);
  this->hub_answers(10);
  this->loops(1);
  EXPECT_EQ(received(this->a_), read_reply(1, 10));

  EXPECT_EQ(this->hub_takes_request(), 2);
  this->hub_answers(20);
  this->loops(1);
  EXPECT_EQ(received(this->b_), read_reply(1, 20));
  EXPECT_TRUE(received(this->a_).empty());
}

TEST_F(ModbusServerTwoClients, SlowReplyStillReachesTheClientThatAsked) {
  // The hub needs 500 ms, longer than the 100 ms the tcp_uart gives a quiet connection.
  send(this->a_, read_request(1, 1));
  EXPECT_EQ(this->hub_takes_request(), 1);
  send(this->b_, read_request(1, 2));
  this->at(1500);
  this->loops(3);
  this->hub_answers(10);
  this->loops(1);
  EXPECT_EQ(received(this->a_), read_reply(1, 10));
  EXPECT_EQ(this->hub_takes_request(), 2);
}

TEST_F(ModbusServerTwoClients, LateReplyAfterTheClientLeftIsDropped) {
  send(this->a_, read_request(1, 1));
  EXPECT_EQ(this->hub_takes_request(), 1);
  ::close(this->a_);
  this->a_ = -1;
  for (int i = 0; i < 50 && this->socket_.open_connections() == 2; i++) {
    this->loops(1);
  }

  // B asks while the hub still works on A's request.
  send(this->b_, read_request(7, 2));
  this->loops(3);
  this->hub_answers(10);
  this->loops(1);
  EXPECT_TRUE(received(this->b_).empty());

  EXPECT_EQ(this->hub_takes_request(), 2);
  this->hub_answers(20);
  this->loops(1);
  EXPECT_EQ(received(this->b_), read_reply(7, 20));
}

TEST_F(ModbusServerTwoClients, RequestsSentAheadKeepTheirOrder) {
  // A sends registers 1 and 2 in one write, then B asks for register 3.
  std::vector<uint8_t> two = read_request(1, 1);
  std::vector<uint8_t> second = read_request(2, 2);
  two.insert(two.end(), second.begin(), second.end());
  send(this->a_, two);
  this->loops(1);
  send(this->b_, read_request(1, 3));

  std::vector<int> order;
  for (uint8_t value : {10, 20, 30}) {
    order.push_back(this->hub_takes_request());
    this->hub_answers(value);
    this->loops(1);
  }
  EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));

  std::vector<uint8_t> want_a = read_reply(1, 10);
  std::vector<uint8_t> reply_a2 = read_reply(2, 20);
  want_a.insert(want_a.end(), reply_a2.begin(), reply_a2.end());
  std::vector<uint8_t> got_a = received(this->a_);
  if (got_a.size() < want_a.size()) {
    std::vector<uint8_t> rest = received(this->a_);
    got_a.insert(got_a.end(), rest.begin(), rest.end());
  }
  EXPECT_EQ(got_a, want_a);
  EXPECT_EQ(received(this->b_), read_reply(1, 30));
}

}  // namespace
}  // namespace esphome::modbus_tcp_uart::testing

#endif
#endif
