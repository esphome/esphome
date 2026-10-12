#include <gtest/gtest.h>

#include <csignal>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>

#include "esphome/components/tcp_uart/tcp_uart.h"

#ifdef USE_HOST

namespace esphome::tcp_uart::testing {

class TcpUartUnderTest : public TcpUart {
 public:
  TcpUartUnderTest() {
    this->set_host("peer");
    this->set_port(1);
    this->link_.begin("flush_test");
  }
  socket::TcpClientLink &link() { return this->link_; }
};

class TcpUartFlush : public ::testing::Test {
 protected:
  void SetUp() override {
    // EPIPE must come back as an errno, not a signal.
    signal(SIGPIPE, SIG_IGN);
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    this->peer_fd_ = fds[1];
    this->uart_.link().adopt(std::make_unique<socket::Socket>(fds[0]));
  }
  void TearDown() override {
    if (this->peer_fd_ >= 0) {
      ::close(this->peer_fd_);
    }
    this->uart_.link().close();
  }

  TcpUartUnderTest uart_;
  int peer_fd_{-1};
};

TEST_F(TcpUartFlush, SuccessWhenTheByteGoesOut) {
  uint8_t b = 'x';
  this->uart_.write_array(&b, 1);
  EXPECT_EQ(this->uart_.flush(), uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS);
  char got;
  EXPECT_EQ(::read(this->peer_fd_, &got, 1), 1);
  EXPECT_EQ(got, 'x');
}

TEST_F(TcpUartFlush, FailedWhenTheFlushDropsTheLink) {
  uint8_t b = 'x';
  this->uart_.write_array(&b, 1);
  ::close(this->peer_fd_);
  this->peer_fd_ = -1;
  // The drop happens inside this flush; checking connected() first would
  // wrongly report success.
  ASSERT_TRUE(this->uart_.is_connected());
  EXPECT_EQ(this->uart_.flush(), uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED);
  EXPECT_FALSE(this->uart_.is_connected());
}

TEST_F(TcpUartFlush, FailedWhileTheLinkIsDown) {
  this->uart_.link().close();
  EXPECT_EQ(this->uart_.flush(), uart::UARTFlushResult::UART_FLUSH_RESULT_FAILED);
}

}  // namespace esphome::tcp_uart::testing

#endif
