#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "esphome/components/uart_tcp/uart_tcp.h"
#include "esphome/core/application.h"
#include "esphome/core/wake.h"

#ifdef USE_HOST

namespace esphome::uart_tcp::testing {

// A UART the test fills and drains directly; available_for_write() is settable.
class FakeUart : public uart::UARTComponent {
 public:
  FakeUart() { this->set_baud_rate(9600); }
  void write_array(const uint8_t *data, size_t len) override {
    this->tx.insert(this->tx.end(), data, data + len);
    this->writes.push_back(len);
  }
  bool peek_byte(uint8_t *data) override {
    if (this->rx.empty())
      return false;
    *data = this->rx.front();
    return true;
  }
  bool read_array(uint8_t *data, size_t len) override {
    if (len > this->rx.size())
      return false;
    std::copy(this->rx.begin(), this->rx.begin() + len, data);
    this->rx.erase(this->rx.begin(), this->rx.begin() + len);
    return true;
  }
  size_t available() override { return this->rx.size(); }
  size_t available_for_write() override { return this->room; }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
  void check_logger_conflict() override {}

  void feed(const char *text) {
    for (const char *p = text; *p != '\0'; p++)
      this->rx.push_back(static_cast<uint8_t>(*p));
  }

  std::vector<uint8_t> rx;
  std::vector<uint8_t> tx;
  std::vector<size_t> writes;
  size_t room{SIZE_MAX};
};

// Client role against a loopback listener the test owns.
class UartTcpClient : public ::testing::Test {
 protected:
  void SetUp() override {
    // EPIPE must come back as an errno, not a signal.
    signal(SIGPIPE, SIG_IGN);
    App.set_loop_interval(16);
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
    this->bridge_.set_connected_sensor(&this->sensor_);
    this->tick(0);
    this->bridge_.setup();
  }
  void TearDown() override {
    this->bridge_.on_shutdown();
    this->close_peer();
    ::close(this->listen_fd_);
    App.set_loop_interval(16);
  }

  // Advance the test clock and publish it as the loop start time, as Application::loop() does.
  void tick(uint32_t elapsed_ms) {
    this->now_ += elapsed_ms;
    LoopBlockingGuard dispatch{nullptr, nullptr, this->now_};
  }
  // One main loop pass that started elapsed_ms after the previous one: select()
  // marks readable sockets, then the component runs.
  void pass(uint32_t elapsed_ms = 16) {
    internal::wakeable_delay(5);
    this->tick(elapsed_ms);
    this->bridge_.loop();
  }
  // Pass until the bridge connected and the test accepted it.
  void connect() {
    for (int i = 0; i < 50 && this->peer_fd_ < 0; i++) {
      this->pass();
      this->peer_fd_ = ::accept(this->listen_fd_, nullptr, nullptr);
    }
    ASSERT_GE(this->peer_fd_, 0);
    for (int i = 0; i < 50 && !this->sensor_.state; i++)
      this->pass();
    ASSERT_TRUE(this->sensor_.state);
  }
  void close_peer() {
    if (this->peer_fd_ >= 0) {
      ::close(this->peer_fd_);
      this->peer_fd_ = -1;
    }
  }
  void send(size_t count) {
    std::vector<uint8_t> data(count);
    for (size_t i = 0; i < count; i++)
      data[i] = static_cast<uint8_t>(i);
    ASSERT_EQ(::write(this->peer_fd_, data.data(), count), static_cast<ssize_t>(count));
  }
  std::string receive(size_t count) {
    std::string out;
    for (int i = 0; i < 50 && out.size() < count; i++) {
      this->pass();
      char buf[64];
      ssize_t n = ::recv(this->peer_fd_, buf, sizeof(buf), MSG_DONTWAIT);
      if (n > 0)
        out.append(buf, static_cast<size_t>(n));
    }
    return out;
  }

  FakeUart uart_;
  UartTcp bridge_;
  binary_sensor::BinarySensor sensor_;
  int listen_fd_{-1};
  int peer_fd_{-1};
  uint32_t now_{100000};
};

TEST_F(UartTcpClient, CopiesBothWays) {
  this->connect();
  this->uart_.feed("up");
  EXPECT_EQ(this->receive(2), "up");
  ASSERT_EQ(::write(this->peer_fd_, "down", 4), 4);
  for (int i = 0; i < 50 && this->uart_.tx.size() < 4; i++)
    this->pass();
  EXPECT_EQ(std::string(this->uart_.tx.begin(), this->uart_.tx.end()), "down");
}

TEST_F(UartTcpClient, DiscardsStaleUartBytesOnConnect) {
  // More than one discard chunk, so the drain loop runs several times.
  for (int i = 0; i < 100; i++)
    this->uart_.rx.push_back('s');
  this->connect();
  EXPECT_TRUE(this->uart_.rx.empty());
  this->uart_.feed("live");
  EXPECT_EQ(this->receive(4), "live");
}

TEST_F(UartTcpClient, ReconnectsAndDropsBytesFromTheGap) {
  this->connect();
  this->close_peer();
  for (int i = 0; i < 50 && this->sensor_.state; i++)
    this->pass();
  ASSERT_FALSE(this->sensor_.state);
  this->uart_.feed("gap");
  this->connect();
  this->uart_.feed("new");
  EXPECT_EQ(this->receive(3), "new");
}

TEST_F(UartTcpClient, PacesToTheDefaultLoopInterval) {
  this->connect();
  this->send(100);
  this->pass();
  this->pass();
  // 9600 baud at 10 bits per byte for 16 ms.
  EXPECT_EQ(this->uart_.writes, (std::vector<size_t>{15, 15}));
}

TEST_F(UartTcpClient, PacesAnEarlyPassToTheTimeSinceTheLastWrite) {
  App.set_loop_interval(100);
  this->connect();
  this->send(200);
  this->pass(100);
  // A socket wake 5 ms later gets 5 ms of UART time, not a full interval.
  this->pass(5);
  this->pass(100);
  EXPECT_EQ(this->uart_.writes, (std::vector<size_t>{96, 4, 96}));
}

TEST_F(UartTcpClient, CapsALongGapAtOneLoopInterval) {
  App.set_loop_interval(100);
  this->connect();
  this->send(250);
  // A 1000 ms gap gets one interval (96 bytes), a 50 ms pass gets 50 ms.
  this->pass(1000);
  this->pass(50);
  this->pass(1000);
  EXPECT_EQ(this->uart_.writes, (std::vector<size_t>{96, 48, 96}));
}

TEST_F(UartTcpClient, CapsTheSpanAtFourSeconds) {
  App.set_loop_interval(10000);
  this->uart_.set_baud_rate(300);
  this->connect();
  this->send(200);
  // 300 baud is 30 bytes/s: a 6000 ms gap gets 4 s, a 1000 ms pass gets 1 s.
  this->pass(6000);
  this->pass(1000);
  EXPECT_EQ(this->uart_.writes, (std::vector<size_t>{120, 30}));
}

TEST_F(UartTcpClient, PacesEachBaudRate) {
  this->connect();
  this->send(1000);
  // 16 ms and 1 ms passes; a write is at most one 128-byte read chunk.
  for (uint32_t baud : {9600, 115200, 921600}) {
    this->uart_.set_baud_rate(baud);
    this->pass(16);
    this->pass(1);
  }
  EXPECT_EQ(this->uart_.writes, (std::vector<size_t>{15, 1, 128, 11, 128, 92}));
}

TEST_F(UartTcpClient, DoesNotOverflowAtAHighBaudRate) {
  App.set_loop_interval(10000);
  // baud * 4000 wraps a 32-bit product to 3520 at this rate.
  this->uart_.set_baud_rate(5368710);
  this->connect();
  this->send(200);
  this->pass(6000);
  EXPECT_EQ(this->uart_.writes, (std::vector<size_t>{128}));
}

TEST_F(UartTcpClient, WritesAtLeastOneBytePerPass) {
  this->connect();
  this->send(100);
  this->pass();
  // Less than one byte of UART time since the last write still moves a byte.
  this->pass(0);
  this->pass(1);
  EXPECT_EQ(this->uart_.writes, (std::vector<size_t>{15, 1, 1}));
}

TEST_F(UartTcpClient, FullUartHoldsSocketBytesUntilThereIsRoom) {
  this->connect();
  this->uart_.room = 0;
  this->send(40);
  for (int i = 0; i < 5; i++)
    this->pass();
  EXPECT_TRUE(this->uart_.tx.empty());
  EXPECT_TRUE(this->sensor_.state);
  this->uart_.room = 16;
  this->pass();
  ASSERT_EQ(this->uart_.writes.size(), 1u);
  EXPECT_EQ(this->uart_.writes.front(), 16u);
  this->uart_.room = SIZE_MAX;
  for (int i = 0; i < 50 && this->uart_.tx.size() < 40; i++)
    this->pass();
  ASSERT_EQ(this->uart_.tx.size(), 40u);
  for (size_t i = 0; i < 40; i++)
    EXPECT_EQ(this->uart_.tx[i], static_cast<uint8_t>(i));
}

}  // namespace esphome::uart_tcp::testing

#endif
