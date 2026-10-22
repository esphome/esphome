#include <gtest/gtest.h>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/tcp_uart/tcp_uart.h"
#include "esphome/core/application.h"
#include "esphome/core/wake/wake_host.h"

#ifdef USE_HOST
#ifdef USE_TCP_UART_MAX_CONNECTIONS

// Host only: ESP-IDF has no poll.h.
#include <poll.h>

namespace esphome::tcp_uart::testing {

class TcpUartConnectionsUnderTest : public TcpUart {
 public:
  uint8_t open_connections() const { return static_cast<uint8_t>(__builtin_popcount(this->up_mask_)); }
};

// A server with two connection slots and a 100 ms turn timeout. Clients A and B are real TCP
// sockets on loopback, connected at 1000 ms.
class TcpUartTwoClients : public ::testing::Test {
 protected:
  void SetUp() override {
    signal(SIGPIPE, SIG_IGN);
    this->at(1000);
    this->port_ = free_port();
    this->uart_.set_server(true);
    this->uart_.set_port(this->port_);
    this->uart_.set_max_connections(2);
    this->uart_.set_turn_timeout(100);
    this->uart_.set_disconnects_sensor(&this->disconnects_);
    this->uart_.setup();
    // Opens the listener.
    this->loops(1);
    this->a_ = this->connect_client();
    this->b_ = this->connect_client();
    ASSERT_EQ(this->uart_.open_connections(), 2);
  }
  void TearDown() override {
    for (int fd : {this->a_, this->b_, this->c_}) {
      if (fd >= 0) {
        ::close(fd);
      }
    }
    this->uart_.on_shutdown();
  }

  static uint16_t free_port() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    socklen_t len = sizeof(addr);
    ::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), len);
    ::getsockname(fd, reinterpret_cast<struct sockaddr *>(&addr), &len);
    ::close(fd);
    return ntohs(addr.sin_port);
  }
  // Sets the loop clock.
  static void at(uint32_t ms) { LoopBlockingGuard{nullptr, nullptr, ms}; }
  // Each pass: select() marks the ready sockets, as the main loop does, then the UART runs.
  void loops(int count) {
    for (int i = 0; i < count; i++) {
      internal::wakeable_delay(5);
      this->uart_.loop();
    }
  }
  // Connects a client and runs the loop until the UART took it.
  int connect_client() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(this->port_);
    EXPECT_EQ(::connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)), 0);
    const uint8_t before = this->uart_.open_connections();
    for (int i = 0; i < 50 && this->uart_.open_connections() == before; i++) {
      this->loops(1);
    }
    return fd;
  }
  static void send(int fd, const char *text) {
    ASSERT_EQ(::write(fd, text, strlen(text)), static_cast<ssize_t>(strlen(text)));
  }
  // What the client received within 200 ms: "" for nothing, "<closed>" for EOF.
  static std::string received(int fd) {
    struct pollfd pfd{fd, POLLIN, 0};
    if (::poll(&pfd, 1, 200) != 1) {
      return "";
    }
    char buf[32];
    ssize_t n = ::read(fd, buf, sizeof(buf));
    return n > 0 ? std::string(buf, n) : "<closed>";
  }
  // Runs the loop until the UART has count bytes to read.
  void wait_for_bytes(size_t count) {
    for (int i = 0; i < 50 && this->uart_.available() < count; i++) {
      this->loops(1);
    }
  }
  std::string read_all() {
    char buf[32]{};
    size_t n = this->uart_.available();
    EXPECT_TRUE(this->uart_.read_array(reinterpret_cast<uint8_t *>(buf), n));
    return std::string(buf, n);
  }
  void write(const char *text) { this->uart_.write_array(reinterpret_cast<const uint8_t *>(text), strlen(text)); }

  TcpUartConnectionsUnderTest uart_;
  sensor::Sensor disconnects_;
  uint16_t port_{0};
  int a_{-1};
  int b_{-1};
  int c_{-1};
};

TEST_F(TcpUartTwoClients, ReplyGoesOnlyToTheClientThatAsked) {
  send(this->a_, "ping");
  this->wait_for_bytes(4);
  EXPECT_EQ(this->read_all(), "ping");
  this->write("pong");
  this->loops(1);
  EXPECT_EQ(received(this->a_), "pong");
  EXPECT_EQ(received(this->b_), "");
}

TEST_F(TcpUartTwoClients, SecondClientWaitsUntilTheFirstIsQuietFor100ms) {
  send(this->a_, "ping");
  this->wait_for_bytes(4);
  EXPECT_EQ(this->read_all(), "ping");
  const uint16_t turn_of_a = this->uart_.turn();

  send(this->b_, "hi");
  this->loops(3);
  EXPECT_EQ(this->uart_.available(), 0u);
  this->at(1099);
  this->loops(3);
  EXPECT_EQ(this->uart_.available(), 0u);

  this->at(1100);
  this->wait_for_bytes(2);
  EXPECT_EQ(this->read_all(), "hi");
  EXPECT_NE(this->uart_.turn(), turn_of_a);
  // The reply goes to the new holder.
  this->write("ok");
  this->loops(1);
  EXPECT_EQ(received(this->b_), "ok");
  EXPECT_EQ(received(this->a_), "");
}

TEST_F(TcpUartTwoClients, HeldTurnStaysUntilReleased) {
  send(this->a_, "ping");
  this->wait_for_bytes(4);
  this->uart_.hold_turn();
  EXPECT_EQ(this->read_all(), "ping");

  send(this->b_, "hi");
  this->at(5000);
  this->loops(3);
  EXPECT_EQ(this->uart_.available(), 0u);

  this->uart_.release_turn();
  this->wait_for_bytes(2);
  EXPECT_EQ(this->read_all(), "hi");
}

TEST_F(TcpUartTwoClients, ReleaseWaitsForTheBytesAlreadyRead) {
  // A sends two requests, "1" and "2", that arrive in one read; both come before B's.
  send(this->a_, "12");
  this->wait_for_bytes(2);
  send(this->b_, "hi");
  this->uart_.hold_turn();
  uint8_t request;
  ASSERT_TRUE(this->uart_.read_byte(&request));
  EXPECT_EQ(request, '1');
  this->uart_.release_turn();
  this->loops(3);

  // The consumer takes "2" and holds the turn again for its reply.
  this->uart_.hold_turn();
  ASSERT_TRUE(this->uart_.read_byte(&request));
  EXPECT_EQ(request, '2');
  this->at(5000);
  this->loops(3);
  EXPECT_EQ(this->uart_.available(), 0u);

  this->uart_.release_turn();
  this->wait_for_bytes(2);
  EXPECT_EQ(this->read_all(), "hi");
}

TEST_F(TcpUartTwoClients, ReleasedTurnReadsNothingNewFromItsConnection) {
  // A sends "1" and "2" in one write, then B sends "hi".
  send(this->a_, "12");
  this->wait_for_bytes(2);
  send(this->b_, "hi");
  this->uart_.hold_turn();
  uint8_t request;
  ASSERT_TRUE(this->uart_.read_byte(&request));
  EXPECT_EQ(request, '1');
  this->uart_.release_turn();
  this->loops(3);

  // The consumer takes "2" and holds the turn for its reply; meanwhile A sends "3".
  this->uart_.hold_turn();
  ASSERT_TRUE(this->uart_.read_byte(&request));
  EXPECT_EQ(request, '2');
  send(this->a_, "3");
  // 20 ms for loopback to deliver "3". The loop passes alone take no time: B's "hi" keeps select() ready.
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  this->loops(3);
  // "3" stays in A's socket for A's next turn.
  EXPECT_EQ(this->uart_.available(), 0u);

  this->uart_.release_turn();
  this->wait_for_bytes(2);
  EXPECT_EQ(this->read_all(), "hi");
}

TEST_F(TcpUartTwoClients, RequestSplitAfterAReleaseKeepsItsTurn) {
  // Requests are two bytes. A writes "1." and "2." and the first byte of "3." at once, then B sends "hi".
  send(this->a_, "1.2.3");
  this->wait_for_bytes(5);
  send(this->b_, "hi");
  const uint16_t turn_of_a = this->uart_.turn();
  char request[3]{};
  for (const char *expected : {"1.", "2."}) {
    this->uart_.hold_turn();
    ASSERT_TRUE(this->uart_.read_array(reinterpret_cast<uint8_t *>(request), 2));
    EXPECT_STREQ(request, expected);
    this->uart_.release_turn();
    this->loops(3);
  }

  // The consumer takes "3" and waits for the rest, which A writes 20 ms later, within the 100 ms turn timeout.
  uint8_t first_byte;
  ASSERT_TRUE(this->uart_.read_byte(&first_byte));
  EXPECT_EQ(first_byte, '3');
  this->loops(3);
  this->at(1020);
  send(this->a_, ".");
  // Loopback needs a moment; B's "hi" keeps select() ready, so the loop passes alone take no time.
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  this->loops(3);
  EXPECT_EQ(this->uart_.turn(), turn_of_a);
  EXPECT_EQ(this->read_all(), ".");

  // Request 3 is answered; then B has its turn.
  this->uart_.hold_turn();
  this->uart_.release_turn();
  this->wait_for_bytes(2);
  EXPECT_EQ(this->read_all(), "hi");
}

TEST_F(TcpUartTwoClients, ClosingDuringItsTurnDropsItsBytes) {
  send(this->a_, "abc");
  this->wait_for_bytes(3);
  const uint16_t turn_of_a = this->uart_.turn();
  ::close(this->a_);
  this->a_ = -1;
  for (int i = 0; i < 50 && this->uart_.open_connections() == 2; i++) {
    this->loops(1);
  }
  EXPECT_EQ(this->uart_.available(), 0u);
  EXPECT_NE(this->uart_.turn(), turn_of_a);
  EXPECT_TRUE(this->uart_.is_connected());
  EXPECT_FLOAT_EQ(this->disconnects_.state, 1);

  send(this->b_, "hi");
  this->wait_for_bytes(2);
  EXPECT_EQ(this->read_all(), "hi");
}

TEST_F(TcpUartTwoClients, WriteAfterTheTurnEndedGoesToTheLastHolder) {
  send(this->a_, "ping");
  this->wait_for_bytes(4);
  this->read_all();
  this->at(1100);
  this->loops(1);
  this->write("late");
  this->loops(1);
  EXPECT_EQ(received(this->a_), "late");
  EXPECT_EQ(received(this->b_), "");
}

TEST_F(TcpUartTwoClients, WriteAfterTheLastHolderClosedNeverReachesTheNextClient) {
  send(this->a_, "ping");
  this->wait_for_bytes(4);
  this->read_all();
  this->at(1100);
  this->loops(1);
  ::close(this->a_);
  this->a_ = -1;
  for (int i = 0; i < 50 && this->uart_.open_connections() == 2; i++) {
    this->loops(1);
  }
  // C takes the slot A left.
  this->c_ = this->connect_client();
  this->write("late");
  this->loops(1);
  EXPECT_EQ(received(this->c_), "");
  EXPECT_EQ(received(this->b_), "");
}

TEST_F(TcpUartTwoClients, FullPoolClosesTheClientQuietLongest) {
  // A connected at 1000 and never sent. B sent at 2000, and its turn ended at 2100.
  this->at(2000);
  send(this->b_, "x");
  this->wait_for_bytes(1);
  this->read_all();
  this->at(2100);
  this->loops(1);

  this->at(3000);
  this->c_ = this->connect_client();
  EXPECT_EQ(received(this->a_), "<closed>");
  EXPECT_EQ(received(this->b_), "");
  EXPECT_EQ(this->uart_.open_connections(), 2);
  EXPECT_FLOAT_EQ(this->disconnects_.state, 1);

  send(this->c_, "new");
  this->wait_for_bytes(3);
  EXPECT_EQ(this->read_all(), "new");
}

}  // namespace esphome::tcp_uart::testing

#endif
#endif
