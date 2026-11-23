#include <gtest/gtest.h>

#include <csignal>
#include <cstring>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"

#ifdef USE_HOST

namespace esphome::tcp_uart::testing {

class TcpUartLoopDriver : public TcpUart {
 public:
  TcpUartLoopDriver() {
    this->set_host("peer");
    this->set_port(1);
    this->link_.begin("buffers_test");
  }
  Link &link() { return this->link_; }
};

class TcpUartBuffers : public ::testing::Test {
 protected:
  void SetUp() override {
    // EPIPE must come back as an errno, not a signal.
    signal(SIGPIPE, SIG_IGN);
    this->connect_peer();
  }
  void TearDown() override {
    this->close_peer();
    this->uart_.link().close();
  }
  // Hands the UART a fresh session, as an accept or a reconnect would.
  void connect_peer() {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    this->peer_fd_ = fds[1];
    this->uart_.link().adopt(std::make_unique<socket::Socket>(fds[0]));
  }
  void close_peer() {
    if (this->peer_fd_ >= 0) {
      ::close(this->peer_fd_);
      this->peer_fd_ = -1;
    }
  }
  void send(const void *data, size_t len) { ASSERT_EQ(::write(this->peer_fd_, data, len), static_cast<ssize_t>(len)); }
  void loops(int count) {
    for (int i = 0; i < count; i++) {
      this->uart_.loop();
    }
  }

  TcpUartLoopDriver uart_;
  int peer_fd_{-1};
};

TEST_F(TcpUartBuffers, BytesReceivedBeforeACloseStayReadable) {
  this->loops(1);
  this->send("HELLO-0123456789", 16);
  this->close_peer();
  // Read the bytes, see the close, run the down edge, then idle.
  this->loops(4);
  ASSERT_FALSE(this->uart_.is_connected());
  ASSERT_EQ(this->uart_.available(), 16u);
  uint8_t got[16];
  ASSERT_TRUE(this->uart_.read_array(got, sizeof(got)));
  EXPECT_EQ(std::memcmp(got, "HELLO-0123456789", sizeof(got)), 0);
  EXPECT_EQ(this->uart_.available(), 0u);
}

TEST_F(TcpUartBuffers, UnreadBytesAreGoneWhenTheNextSessionStarts) {
  this->loops(1);
  this->send("OLD", 3);
  this->close_peer();
  this->loops(4);
  ASSERT_FALSE(this->uart_.is_connected());
  this->connect_peer();
  this->send("NEW", 3);
  this->loops(1);
  ASSERT_EQ(this->uart_.available(), 3u);
  uint8_t got[3];
  ASSERT_TRUE(this->uart_.read_array(got, sizeof(got)));
  EXPECT_EQ(std::memcmp(got, "NEW", sizeof(got)), 0);
}

TEST_F(TcpUartBuffers, FullBufferWaitsAndCompactsAfterARead) {
  static constexpr size_t TOTAL = 1500;
  uint8_t data[TOTAL];
  for (size_t i = 0; i < TOTAL; i++) {
    data[i] = static_cast<uint8_t>(i % 251);
  }
  this->loops(1);
  this->send(data, TOTAL);
  this->loops(1);
  ASSERT_EQ(this->uart_.available(), 1024u);
  // Nothing read, no room: the rest stays in the socket.
  this->loops(2);
  ASSERT_EQ(this->uart_.available(), 1024u);

  uint8_t got[TOTAL];
  ASSERT_TRUE(this->uart_.read_array(got, 100));
  // The read freed the front; the next pass moves the rest down and refills.
  this->loops(1);
  ASSERT_EQ(this->uart_.available(), 1024u);
  ASSERT_TRUE(this->uart_.read_array(got + 100, 1024));
  this->loops(1);
  ASSERT_EQ(this->uart_.available(), TOTAL - 1124);
  ASSERT_TRUE(this->uart_.read_array(got + 1124, TOTAL - 1124));
  EXPECT_EQ(std::memcmp(got, data, TOTAL), 0);
  EXPECT_TRUE(this->uart_.is_connected());
}

TEST_F(TcpUartBuffers, WritesBeyondTheSendBufferAreDropped) {
  static constexpr size_t TOTAL = 1500;
  uint8_t data[TOTAL];
  for (size_t i = 0; i < TOTAL; i++) {
    data[i] = static_cast<uint8_t>(i % 251);
  }
  this->loops(1);
  this->uart_.write_array(data, TOTAL);
  EXPECT_EQ(this->uart_.available_for_write(), 0u);
  EXPECT_EQ(this->uart_.flush(), uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS);

  uint8_t got[TOTAL];
  size_t total = 0;
  while (total < 1024) {
    ssize_t n = ::read(this->peer_fd_, got + total, sizeof(got) - total);
    ASSERT_GT(n, 0);
    total += static_cast<size_t>(n);
  }
  EXPECT_EQ(total, 1024u);
  EXPECT_EQ(std::memcmp(got, data, 1024), 0);
  // The 476 bytes past the buffer never reach the peer.
  EXPECT_EQ(::recv(this->peer_fd_, got, sizeof(got), MSG_DONTWAIT), -1);
}

#ifdef USE_SOCKET_TCP_LISTENER
// Server role through the real listener on a loopback port.
class TcpUartServer : public ::testing::Test {
 protected:
  void SetUp() override {
    signal(SIGPIPE, SIG_IGN);
    ASSERT_NO_FATAL_FAILURE(find_free_port(&this->port_));
    this->uart_.set_server(true);
    this->uart_.set_port(this->port_);
    this->uart_.set_reconnect_interval(0);
    this->uart_.set_idle_timeout(this->idle_timeout_);
    this->uart_.setup();
    this->pass();
  }
  void TearDown() override {
    this->close_peer();
    this->uart_.on_shutdown();
  }
  // One main loop pass, 16 ms after the last.
  void pass() {
    this->now_ += 16;
    set_loop_clock(this->now_);
    loop_pass(this->uart_);
  }
  void connect_peer() {
    ASSERT_NO_FATAL_FAILURE(connect_loopback(this->port_, &this->peer_fd_));
    for (int i = 0; i < 50 && !this->uart_.is_connected(); i++)
      this->pass();
    ASSERT_TRUE(this->uart_.is_connected());
  }
  void close_peer() {
    if (this->peer_fd_ >= 0) {
      ::close(this->peer_fd_);
      this->peer_fd_ = -1;
    }
  }
  void send(const void *data, size_t len) { send_bytes(this->peer_fd_, data, len); }
  void pass_until_available(size_t count) {
    testing::pass_until_available(this->uart_, count, [this]() { this->pass(); });
  }

  TcpUart uart_;
  uint16_t port_{0};
  int peer_fd_{-1};
  uint32_t now_{0};
  // 0 leaves a quiet client connected.
  uint32_t idle_timeout_{0};
};

TEST_F(TcpUartServer, NextAcceptedClientStartsWithAnEmptyBuffer) {
  this->connect_peer();
  this->send("OLD", 3);
  this->pass_until_available(3);
  ASSERT_EQ(this->uart_.available(), 3u);
  this->close_peer();
  for (int i = 0; i < 50 && this->uart_.is_connected(); i++)
    this->pass();
  ASSERT_FALSE(this->uart_.is_connected());
  // Unread bytes stay readable while no client is connected.
  EXPECT_EQ(this->uart_.available(), 3u);

  this->connect_peer();
  this->send("NEW", 3);
  this->pass_until_available(3);
  ASSERT_EQ(this->uart_.available(), 3u);
  uint8_t got[3];
  ASSERT_TRUE(this->uart_.read_array(got, sizeof(got)));
  EXPECT_EQ(std::memcmp(got, "NEW", sizeof(got)), 0);
}

// The one-connection server above with a 100 ms idle timeout. Each pass is 16 ms.
class TcpUartServerIdleTimeout : public TcpUartServer {
 protected:
  TcpUartServerIdleTimeout() { this->idle_timeout_ = 100; }
};

TEST_F(TcpUartServerIdleTimeout, HeldTurnKeepsTheOnlyClientOpen) {
  // The client asks, and the consumer holds the turn until its reply is ready.
  this->connect_peer();
  this->send("ping", 4);
  this->pass_until_available(4);
  uint8_t got[4];
  ASSERT_TRUE(this->uart_.read_array(got, sizeof(got)));
  this->uart_.hold_turn();
  // The reply takes 20 passes: 320 ms without a byte, longer than the timeout.
  for (int i = 0; i < 20; i++) {
    this->pass();
  }
  ASSERT_TRUE(this->uart_.is_connected());
  this->uart_.write_array(reinterpret_cast<const uint8_t *>("pong"), 4);
  this->uart_.release_turn();
  this->pass();
  ASSERT_EQ(::recv(this->peer_fd_, got, sizeof(got), MSG_WAITALL), 4);
  EXPECT_EQ(std::memcmp(got, "pong", sizeof(got)), 0);

  // Once answered, the client is quiet like any other: 7 passes (112 ms) later it is closed.
  for (int i = 0; i < 7; i++) {
    this->pass();
  }
  EXPECT_FALSE(this->uart_.is_connected());
}

TEST_F(TcpUartServerIdleTimeout, HoldEndsWithItsSession) {
  // The first client asks and leaves while the turn is held. Its turn is never released.
  this->connect_peer();
  this->send("ping", 4);
  this->pass_until_available(4);
  this->uart_.hold_turn();
  this->close_peer();
  for (int i = 0; i < 50 && this->uart_.is_connected(); i++) {
    this->pass();
  }
  ASSERT_FALSE(this->uart_.is_connected());

  // The next client sends nothing: the old hold does not count for it, so it is closed 7 passes (112 ms) later.
  this->connect_peer();
  for (int i = 0; i < 7; i++) {
    this->pass();
  }
  EXPECT_FALSE(this->uart_.is_connected());
}
#endif

}  // namespace esphome::tcp_uart::testing

#endif
