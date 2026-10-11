#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart_tcp/uart_tcp.h"
#include "esphome/core/application.h"
#include "esphome/core/wake.h"

#ifdef USE_HOST

namespace esphome::uart_tcp::testing {

// An idle UART: nothing to read, unknown free TX space.
class IdleUart : public uart::UARTComponent {
 public:
  IdleUart() { this->set_baud_rate(9600); }
  void write_array(const uint8_t *data, size_t len) override {}
  bool peek_byte(uint8_t *data) override { return false; }
  bool read_array(uint8_t *data, size_t len) override { return len == 0; }
  size_t available() override { return 0; }
  size_t available_for_write() override { return SIZE_MAX; }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
  void check_logger_conflict() override {}
};

// Client role against a loopback listener the test owns.
class UartTcpDisconnect : public ::testing::Test {
 protected:
  void SetUp() override {
    signal(SIGPIPE, SIG_IGN);
    this->listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(this->listen_fd_, 0);
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ASSERT_EQ(::bind(this->listen_fd_, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)), 0);
    ASSERT_EQ(::listen(this->listen_fd_, 1), 0);
    ASSERT_EQ(::fcntl(this->listen_fd_, F_SETFL, O_NONBLOCK), 0);
    socklen_t len = sizeof(addr);
    ASSERT_EQ(::getsockname(this->listen_fd_, reinterpret_cast<struct sockaddr *>(&addr), &len), 0);

    this->bridge_.set_uart_parent(&this->uart_);
    this->bridge_.set_host("127.0.0.1");
    this->bridge_.set_port(ntohs(addr.sin_port));
    // A zero interval lets a dropped link retry on the next pass.
    this->bridge_.set_reconnect_interval(0);
    this->bridge_.set_connected_sensor(&this->connected_);
    this->bridge_.set_disconnects_sensor(&this->disconnects_);
    this->tick();
    this->bridge_.setup();
  }
  void TearDown() override {
    this->bridge_.on_shutdown();
    this->close_peer();
    ::close(this->listen_fd_);
  }

  // Publish the next loop start time, as Application::loop() does.
  void tick() {
    this->now_ += 16;
    LoopBlockingGuard dispatch{nullptr, nullptr, this->now_};
  }
  // One main loop pass: select() marks readable sockets, then the component runs.
  void pass() {
    internal::wakeable_delay(5);
    this->tick();
    this->bridge_.loop();
  }
  void connect() {
    for (int i = 0; i < 50 && this->peer_fd_ < 0; i++) {
      this->pass();
      this->peer_fd_ = ::accept(this->listen_fd_, nullptr, nullptr);
    }
    ASSERT_GE(this->peer_fd_, 0);
    for (int i = 0; i < 50 && !this->connected_.state; i++)
      this->pass();
    ASSERT_TRUE(this->connected_.state);
  }
  // The peer closes; pass until the bridge has run its down edge.
  void peer_closes() {
    this->close_peer();
    for (int i = 0; i < 50 && this->connected_.state; i++)
      this->pass();
    ASSERT_FALSE(this->connected_.state);
  }
  void close_peer() {
    if (this->peer_fd_ >= 0) {
      ::close(this->peer_fd_);
      this->peer_fd_ = -1;
    }
  }

  IdleUart uart_;
  UartTcp bridge_;
  binary_sensor::BinarySensor connected_;
  sensor::Sensor disconnects_;
  int listen_fd_{-1};
  int peer_fd_{-1};
  uint32_t now_{0};
};

TEST_F(UartTcpDisconnect, StartsAtZero) { EXPECT_FLOAT_EQ(this->disconnects_.state, 0); }

TEST_F(UartTcpDisconnect, CountsEveryCloseByThePeerAcrossReconnects) {
  for (int session = 1; session <= 3; session++) {
    this->connect();
    this->peer_closes();
    EXPECT_FLOAT_EQ(this->disconnects_.state, session);
  }
}

}  // namespace esphome::uart_tcp::testing

#endif
