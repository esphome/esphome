#include <gtest/gtest.h>

#include <cmath>
#include <csignal>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/tcp_uart/tcp_uart.h"

#ifdef USE_HOST

namespace esphome::tcp_uart::testing {

class TcpUartDisconnectUnderTest : public TcpUart {
 public:
  TcpUartDisconnectUnderTest() {
    this->set_host("peer");
    this->set_port(1);
    this->link_.begin("disconnect_test");
  }
  socket::TcpClientLink &link() { return this->link_; }
};

class TcpUartDisconnect : public ::testing::Test {
 protected:
  void SetUp() override {
    signal(SIGPIPE, SIG_IGN);
    this->uart_.set_disconnects_sensor(&this->disconnects_);
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
  void loops(int count) {
    for (int i = 0; i < count; i++) {
      this->uart_.loop();
    }
  }

  TcpUartDisconnectUnderTest uart_;
  sensor::Sensor disconnects_;
  int peer_fd_{-1};
};

TEST_F(TcpUartDisconnect, CountsTheFallingEdgeOnce) {
  this->uart_.loop();
  EXPECT_TRUE(this->uart_.is_connected());
  EXPECT_TRUE(std::isnan(this->disconnects_.state));

  this->uart_.link().close();
  this->uart_.loop();
  EXPECT_FALSE(this->uart_.is_connected());
  EXPECT_FLOAT_EQ(this->disconnects_.state, 1);

  this->uart_.loop();
  EXPECT_FLOAT_EQ(this->disconnects_.state, 1);
}

TEST_F(TcpUartDisconnect, CountsACloseByThePeer) {
  this->loops(1);
  this->close_peer();
  // Read the EOF, then run the down edge, then idle.
  this->loops(3);
  EXPECT_FALSE(this->uart_.is_connected());
  EXPECT_FLOAT_EQ(this->disconnects_.state, 1);
}

TEST_F(TcpUartDisconnect, CountsEverySessionAfterAReconnect) {
  for (int session = 1; session <= 3; session++) {
    if (session > 1) {
      this->connect_peer();
    }
    this->loops(1);
    ASSERT_TRUE(this->uart_.is_connected());
    this->close_peer();
    this->loops(3);
    ASSERT_FALSE(this->uart_.is_connected());
    EXPECT_FLOAT_EQ(this->disconnects_.state, session);
  }
}

}  // namespace esphome::tcp_uart::testing

#endif
