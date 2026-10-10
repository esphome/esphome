#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <csignal>
#include <cstdint>
#include <fcntl.h>
#include <memory>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "esphome/components/uart_tcp/uart_tcp.h"
#include "esphome/core/application.h"
#include "esphome/core/wake.h"

#if defined(USE_HOST) && defined(USE_NOISE_STREAM)

namespace esphome::uart_tcp::testing {

static const uint8_t KEY[32] = {7, 6, 5,  4,  3,  2,  1,  0,  9,  8,  7,  6,  5,  4,  3,  2,
                                1, 0, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};

// A UART the test fills and drains directly.
class NoiseFakeUart : public uart::UARTComponent {
 public:
  NoiseFakeUart() { this->set_baud_rate(115200); }
  void write_array(const uint8_t *data, size_t len) override { this->tx.insert(this->tx.end(), data, data + len); }
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
  size_t available_for_write() override { return SIZE_MAX; }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS; }
  void check_logger_conflict() override {}

  std::vector<uint8_t> rx;
  std::vector<uint8_t> tx;
};

// The bridge dials a loopback listener the test owns; the test's stream answers.
class UartTcpNoise : public ::testing::Test {
 protected:
  void SetUp() override {
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
    this->bridge_.set_reconnect_interval(0);
    this->bridge_.set_noise_stream(&this->bridge_stream_);
    this->bridge_.set_connected_sensor(&this->sensor_);
    this->tick(0);
    this->bridge_.setup();
    this->peer_.begin("peer");
  }
  void TearDown() override {
    this->bridge_.on_shutdown();
    this->peer_.close();
    ::close(this->listen_fd_);
  }
  void tick(uint32_t elapsed_ms) {
    this->now_ += elapsed_ms;
    LoopBlockingGuard dispatch{nullptr, nullptr, this->now_};
  }
  // One main loop pass for the bridge, then the test's end.
  void pass() {
    internal::wakeable_delay(5);
    this->tick(16);
    this->bridge_.loop();
    if (!this->peer_.connected()) {
      int fd = ::accept(this->listen_fd_, nullptr, nullptr);
      if (fd >= 0)
        this->peer_.adopt(std::make_unique<socket::Socket>(fd));
    }
    this->peer_stream_.up(this->peer_);
    this->peer_stream_.flush(this->peer_);
  }
  void connect() {
    for (int i = 0; i < 50 && !this->sensor_.state; i++)
      this->pass();
    ASSERT_TRUE(this->sensor_.state);
    ASSERT_TRUE(this->peer_stream_.ready());
  }
  std::string receive(size_t count) {
    std::string out;
    for (int i = 0; i < 100 && out.size() < count; i++) {
      this->pass();
      uint8_t buf[256];
      ssize_t n = this->peer_stream_.read(this->peer_, buf, sizeof(buf));
      if (n > 0)
        out.append(reinterpret_cast<char *>(buf), static_cast<size_t>(n));
    }
    return out;
  }

  NoiseFakeUart uart_;
  noise::NoiseStream bridge_stream_{KEY, true};
  UartTcp bridge_;
  binary_sensor::BinarySensor sensor_;
  noise::NoiseStream peer_stream_{KEY, false};
  socket::TcpClientLink peer_;
  int listen_fd_{-1};
  uint32_t now_{100000};
};

TEST_F(UartTcpNoise, CopiesBothWaysOverTheSession) {
  this->connect();
  std::string up(1000, 'u');
  this->uart_.rx.assign(up.begin(), up.end());
  EXPECT_EQ(this->receive(up.size()), up);

  std::string down(700, 'd');
  ASSERT_EQ(this->peer_stream_.queue(this->peer_, reinterpret_cast<const uint8_t *>(down.data()), down.size()),
            down.size());
  for (int i = 0; i < 100 && this->uart_.tx.size() < down.size(); i++)
    this->pass();
  EXPECT_EQ(std::string(this->uart_.tx.begin(), this->uart_.tx.end()), down);
}

TEST_F(UartTcpNoise, BytesFromTheHandshakeAreDropped) {
  // The peer answers the handshake in this pass; the bridge reads the answer in its next loop()
  for (int i = 0; i < 50 && !this->peer_stream_.ready(); i++)
    this->pass();
  ASSERT_TRUE(this->peer_stream_.ready());
  ASSERT_FALSE(this->sensor_.state);
  // TCP is up, the session is not yet
  this->uart_.rx.assign({'s', 't', 'a', 'l', 'e'});
  this->connect();
  EXPECT_TRUE(this->uart_.rx.empty());
  this->uart_.rx.assign({'l', 'i', 'v', 'e'});
  EXPECT_EQ(this->receive(4), "live");
}

}  // namespace esphome::uart_tcp::testing

#endif
